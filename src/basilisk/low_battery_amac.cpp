#if defined(BOARD_AMAC_P4)
#include "sysdeps.h"
#include "board_config.h"
#include "board_power.h"
#include "board_display.h"
#include "board_sd.h"
#include "power_manager.h"
#include "low_battery.h"
#include "amac_charge_wake.h"
#include "mac_shutdown.h"
#include "mac_clock.h"
#include "macos_util.h"
#include "automation.h"
#include "birthday_gift_runtime.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include <WiFi.h>

extern void Sys_flush_now();
extern void SaveXPRAM();
extern "C" void AmacDisplayPrepareDeepSleep();
extern "C" bool AmacDisplayRecoverFromSleepFailure();

bool low_battery_control_pending = false;
namespace {
RTC_DATA_ATTR AmacChargeWakeState charger_wait = {};
portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
LowBatteryPolicy policy;
LowBatteryAction pending = LowBatteryAction::None;
bool shutdown_requested = false;
bool shutdown_complete = false;
bool automatic_shutdown = false;
bool poweroff_in_progress = false;
bool maintenance_hold = false;
uint32_t maintenance_since = 0;
uint32_t shutdown_at = 0;
bool sampled = false;
uint16_t pack_mv = 0;
uint32_t last_sample = 0;
uint32_t last_shutdown_attempt = 0;
uint32_t shutdown_attempts = 0;
int16_t shutdown_error = 0;
const char *wake_reason = "cold";
uint32_t charge_checks = 0;
constexpr uint32_t kManualOffGraceMs = 10000;
constexpr uint32_t kMaintenanceWindowMs = 600000;

void releaseSleepPins()
{
    rtc_gpio_hold_dis(GPIO_NUM_13);
    rtc_gpio_deinit(GPIO_NUM_13);
    gpio_hold_dis(GPIO_NUM_23);
    gpio_hold_dis(GPIO_NUM_54);
}

void enterDeepSleep()
{
    const esp_err_t timer = esp_sleep_enable_timer_wakeup(LowBatteryPolicy::kCheckIntervalUs);
    if (timer != ESP_OK) {
        charger_wait.active = false;
        wake_reason = "sleep_failed";
        releaseSleepPins();
        Serial.printf("[POWER] RTC wake timer failed: %s\n", esp_err_to_name(timer));
        return;
    }
    MacClockPrepareDeepSleep();
    pinMode(AMAC_WIFI_RESET_GPIO, OUTPUT);
    digitalWrite(AMAC_WIFI_RESET_GPIO, LOW);
    gpio_hold_en(GPIO_NUM_54);
    pinMode(AMAC_LCD_RESET_GPIO, OUTPUT);
    digitalWrite(AMAC_LCD_RESET_GPIO, LOW);
    gpio_hold_en(GPIO_NUM_23);
    rtc_gpio_init(GPIO_NUM_13);
    rtc_gpio_set_direction(GPIO_NUM_13, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_NUM_13, !AMAC_BACKLIGHT_ON_LEVEL);
    rtc_gpio_hold_en(GPIO_NUM_13);
    Serial.printf("[POWER] Deep sleep; charger check in 5 s, armed=%d\n", charger_wait.armed);
    Serial.flush();
    const esp_err_t err = esp_deep_sleep_try_to_start();
    charger_wait.active = false;
    wake_reason = "sleep_failed";
    releaseSleepPins();
    Serial.printf("[POWER] Deep sleep rejected: %s\n", esp_err_to_name(err));
}
} // namespace

const char *LowBatteryWakeReason() { return wake_reason; }
uint32_t LowBatteryChargeChecks() { return charge_checks; }

void LowBatteryDescribe(char *buffer, size_t size)
{
    portENTER_CRITICAL(&state_lock);
    const bool deep = pending == LowBatteryAction::DeepSleep;
    const bool requested = shutdown_requested;
    const uint32_t attempts = shutdown_attempts;
    const int16_t error = shutdown_error;
    portEXIT_CRITICAL(&state_lock);
    snprintf(buffer, size, "off_pending=%d off_requested=%d off_attempts=%lu off_error=%d",
             deep, requested, (unsigned long)attempts, error);
}

void LowBatteryBootCheck()
{
    const bool checking = esp_reset_reason() == ESP_RST_DEEPSLEEP && charger_wait.active;
    // Keep C5/LCD holds intact during RTC checks. Releasing them every 5 s
    // would briefly power up peripherals even when returning straight to sleep.
    if (!checking) {
        releaseSleepPins();
        pinMode(AMAC_LCD_BACKLIGHT_GPIO, OUTPUT);
        digitalWrite(AMAC_LCD_BACKLIGHT_GPIO, !AMAC_BACKLIGHT_ON_LEVEL);
    }
    BoardBatteryRaw battery = {};
    bool valid = BoardPowerHW_Read(&battery) && battery.present;
    if (checking) {
        ++charger_wait.checks;
        charge_checks = charger_wait.checks;
        const bool charging = valid && battery.charging;
        // Invalid ADC cannot prove the charger was removed.
        bool wake = valid && charger_wait.observe(charging);
        if (wake) {
            delay(100); // Reject a transient CHRG assertion before enabling peripherals.
            BoardBatteryRaw confirmed = {};
            wake = BoardPowerHW_Read(&confirmed) && confirmed.present && confirmed.charging;
        }
        if (!wake) {
            enterDeepSleep();
            return;
        }
        charger_wait.active = false;
        wake_reason = "charge";
        releaseSleepPins();
        pinMode(AMAC_LCD_BACKLIGHT_GPIO, OUTPUT);
        digitalWrite(AMAC_LCD_BACKLIGHT_GPIO, !AMAC_BACKLIGHT_ON_LEVEL);
        Serial.printf("[POWER] Charging detected; boot after %lu RTC checks\n", (unsigned long)charge_checks);
        return;
    }
    charger_wait.active = false;
    if (!valid) return;
    // Confirm a low, uncharged cold boot before touching LCD/SD/Wi-Fi.
    LowBatteryPolicy boot_policy;
    const uint32_t start = millis();
    while (battery.present && !battery.charging && battery.pack_mv < LowBatteryPolicy::kDeepSleepMv) {
        if (boot_policy.observe(battery.pack_mv, millis(), battery.charging) == LowBatteryAction::DeepSleep) {
            charger_wait.begin(false);
            enterDeepSleep();
            break;
        }
        delay(1000);
        if (!BoardPowerHW_Read(&battery) || millis() - start > LowBatteryPolicy::kConfirmMs + 2000)
            break;
    }
}

void LowBatteryPoll()
{
    const uint32_t now = millis();
    if (sampled && now - last_sample < 1000) return;
    sampled = true;
    last_sample = now;
    BoardBatteryRaw battery = {};
    if (!BoardPowerHW_Read(&battery)) battery.present = false;
    portENTER_CRITICAL(&state_lock);
    pack_mv = battery.present ? battery.pack_mv : 0;
    if (battery.charging || pack_mv >= LowBatteryPolicy::kDeepSleepMv) shutdown_requested = false;
    pending = policy.observe(pack_mv, now, battery.charging);
    __atomic_store_n(&low_battery_control_pending, pending != LowBatteryAction::None, __ATOMIC_RELEASE);
    portEXIT_CRITICAL(&state_lock);
}

bool LowBatteryDeepSleepRequested()
{
    portENTER_CRITICAL(&state_lock);
    const bool deep = pending == LowBatteryAction::DeepSleep;
    portEXIT_CRITICAL(&state_lock);
    return deep;
}

uint16_t LowBatteryVoltageMv()
{
    portENTER_CRITICAL(&state_lock);
    const uint16_t mv = pack_mv;
    portEXIT_CRITICAL(&state_lock);
    return mv;
}

void LowBatteryControlPoll()
{
    __atomic_store_n(&low_battery_control_pending, false, __ATOMIC_RELEASE);
}

void LowBatteryGuestIdle()
{
    if (!LowBatteryDeepSleepRequested() || !HasMacStarted()) return;
    portENTER_CRITICAL(&state_lock);
    const bool requested = shutdown_requested;
    portEXIT_CRITICAL(&state_lock);
    if (requested) return;
    const uint32_t now = millis();
    if (last_shutdown_attempt && now - last_shutdown_attempt < 60000) return;
    last_shutdown_attempt = now;
    BirthdayGift_Close();
    const int16_t err = MacRequestShutdown();
    portENTER_CRITICAL(&state_lock);
    ++shutdown_attempts;
    shutdown_error = err;
    shutdown_requested = err == 0;
    portEXIT_CRITICAL(&state_lock);
    Serial.printf("[POWER] Battery below 4100 mV and not charging: Finder shutdown request, result=%d\n", err);
}

bool LowBatteryPrepareMaintenance()
{
    portENTER_CRITICAL(&state_lock);
    const bool allowed = !poweroff_in_progress;
    if (allowed) {
        maintenance_hold = true;
        maintenance_since = millis();
        if (shutdown_complete && !automatic_shutdown) shutdown_complete = false;
    }
    portEXIT_CRITICAL(&state_lock);
    return allowed;
}

void LowBatteryGuestShutdown()
{
    portENTER_CRITICAL(&state_lock);
    automatic_shutdown = shutdown_requested || pending == LowBatteryAction::DeepSleep;
    const bool hold = maintenance_hold && millis() - maintenance_since < kMaintenanceWindowMs;
    shutdown_complete = automatic_shutdown || !hold;
    maintenance_hold = false; // Applies to one completed guest shutdown only.
    shutdown_at = millis();
    const bool sleeping = shutdown_complete;
    portEXIT_CRITICAL(&state_lock);
    Serial.println(sleeping ? "[POWER] Guest shut down; preparing deep sleep" :
                                      "[POWER] Guest shut down; USB maintenance hold");
}

void LowBatteryEnterAfterShutdown()
{
    portENTER_CRITICAL(&state_lock);
    const bool ready = shutdown_complete &&
        (automatic_shutdown || millis() - shutdown_at >= kManualOffGraceMs);
    if (ready) {
        shutdown_complete = false;
        poweroff_in_progress = true;
    }
    portEXIT_CRITICAL(&state_lock);
    if (!ready) return;
    if (!AutomationLockDisplay()) {
        portENTER_CRITICAL(&state_lock);
        shutdown_complete = true;
        poweroff_in_progress = false;
        portEXIT_CRITICAL(&state_lock);
        return;
    }
    Sys_flush_now();
    SaveXPRAM();
    MacClockSave(true);
    BoardBatteryRaw battery = {};
    const bool valid = BoardPowerHW_Read(&battery) && battery.present;
    charger_wait.begin(!valid || battery.charging); // Unknown cannot establish unplugged.
    SD_MMC.end();
    WiFi.mode(WIFI_OFF);
    AmacDisplayPrepareDeepSleep();
    enterDeepSleep();
    // Only sleep failure returns. Stay recoverable on the safe-to-power-off screen.
    if (!AmacDisplayRecoverFromSleepFailure()) {
        Serial.println("[POWER] Display recovery failed after rejected deep sleep");
        while (true) delay(1000);
    }
    AutomationUnlockDisplay();
    portENTER_CRITICAL(&state_lock);
    poweroff_in_progress = false;
    portEXIT_CRITICAL(&state_lock);
}
#endif
