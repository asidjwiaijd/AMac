#if defined(BOARD_AMAC_P4)
#include "sysdeps.h"
#include "board_config.h"
#include "board_power.h"
#include "board_display.h"
#include "board_sd.h"
#include "power_manager.h"
#include "low_battery.h"
#include "mac_shutdown.h"
#include "mac_clock.h"
#include "macos_util.h"
#include "automation.h"
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
RTC_DATA_ATTR bool voltage_sleep = false;
portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
LowBatteryPolicy policy;
LowBatteryAction pending = LowBatteryAction::None;
bool shutdown_requested = false;
bool shutdown_complete = false;
bool sampled = false;
uint16_t pack_mv = 0;
uint32_t last_sample = 0;
uint32_t last_shutdown_attempt = 0;

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
        Serial.printf("[POWER] RTC wake timer failed: %s\n", esp_err_to_name(timer));
        return;
    }
    MacClockPrepareDeepSleep();
    // C5 EN and LCD reset have dedicated nets. Hold them inactive while P4 sleeps.
    digitalWrite(AMAC_WIFI_RESET_GPIO, LOW);
    pinMode(AMAC_WIFI_RESET_GPIO, OUTPUT);
    gpio_hold_en(GPIO_NUM_54);
    digitalWrite(AMAC_LCD_RESET_GPIO, LOW);
    pinMode(AMAC_LCD_RESET_GPIO, OUTPUT);
    gpio_hold_en(GPIO_NUM_23);
    // GPIO13 is an LP pad. RTC hold also works on pre-v3 P4 silicon.
    rtc_gpio_init(GPIO_NUM_13);
    rtc_gpio_set_direction(GPIO_NUM_13, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_NUM_13, !AMAC_BACKLIGHT_ON_LEVEL);
    rtc_gpio_hold_en(GPIO_NUM_13);
    voltage_sleep = true;
    Serial.println("[POWER] Deep sleep; RTC voltage check in 300 s");
    Serial.flush();
    const esp_err_t err = esp_deep_sleep_try_to_start();
    // Only a rejected sleep returns. Keep recovery / diagnostics possible.
    voltage_sleep = false;
    releaseSleepPins();
    Serial.printf("[POWER] Deep sleep rejected: %s\n", esp_err_to_name(err));
}
} // namespace

void LowBatteryBootCheck()
{
    releaseSleepPins();
    digitalWrite(AMAC_LCD_BACKLIGHT_GPIO, !AMAC_BACKLIGHT_ON_LEVEL);
    pinMode(AMAC_LCD_BACKLIGHT_GPIO, OUTPUT);
    const bool checking = esp_reset_reason() == ESP_RST_DEEPSLEEP && voltage_sleep;
    BoardBatteryRaw battery = {};
    if (!BoardPowerHW_Read(&battery) || !battery.present) {
        if (checking) {
            enterDeepSleep();
            return;
        }
        voltage_sleep = false;
        return;
    }
    // A timer check keeps LCD, SD and C5 uninitialized while the pack recovers.
    if (checking && battery.pack_mv < LowBatteryPolicy::kResumeMv) {
        enterDeepSleep();
        return;
    }
    voltage_sleep = false;
    // Confirm a critically low cold boot before mounting any guest disks.
    LowBatteryPolicy boot_policy;
    uint32_t start = millis();
    while (battery.present && battery.pack_mv < LowBatteryPolicy::kDeepSleepMv) {
        if (boot_policy.observe(battery.pack_mv, millis()) == LowBatteryAction::DeepSleep) {
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
    if (pack_mv >= LowBatteryPolicy::kResumeMv) shutdown_requested = false;
    LowBatteryAction action = policy.observe(battery.present ? battery.pack_mv : 0, now);
    if (action == LowBatteryAction::Standby && (PowerManagerSleeping() || shutdown_requested))
        action = LowBatteryAction::None;
    pending = action;
    __atomic_store_n(&low_battery_control_pending, action != LowBatteryAction::None, __ATOMIC_RELEASE);
    portEXIT_CRITICAL(&state_lock);
}

void LowBatteryNoteActivity()
{
    portENTER_CRITICAL(&state_lock);
    policy.noteActivity(millis());
    if (pending == LowBatteryAction::Standby) {
        pending = LowBatteryAction::None;
        __atomic_store_n(&low_battery_control_pending, false, __ATOMIC_RELEASE);
    }
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
    const uint32_t activity = PowerManagerActivityGeneration();
    portENTER_CRITICAL(&state_lock);
    const bool standby = pending == LowBatteryAction::Standby;
    __atomic_store_n(&low_battery_control_pending, false, __ATOMIC_RELEASE);
    if (standby) pending = LowBatteryAction::None;
    portEXIT_CRITICAL(&state_lock);
    if (!standby) return;
    Sys_flush_now();
    SaveXPRAM();
    MacClockSave(true);
    Serial.println("[POWER] Battery below 3900 mV: standby, USB keyboard/mouse wake");
    PowerManagerSleepUntilWake(activity);
}

void LowBatteryGuestIdle()
{
    if (!LowBatteryDeepSleepRequested() || !HasMacStarted()) return;
    if (shutdown_requested) return;
    const uint32_t now = millis();
    if (last_shutdown_attempt && now - last_shutdown_attempt < 60000) return;
    last_shutdown_attempt = now;
    // Finder sends Quit to applications, then unmounts its HFS volumes.
    // A save/cancel dialog remains usable; deep sleep waits for actual PowerOff.
    const int16_t err = MacRequestShutdown();
    portENTER_CRITICAL(&state_lock);
    shutdown_requested = err == 0;
    portEXIT_CRITICAL(&state_lock);
    Serial.printf("[POWER] Battery below 3700 mV: Finder shutdown request, result=%d\n", err);
}

void LowBatteryGuestShutdown()
{
    shutdown_complete = shutdown_requested;
}

void LowBatteryEnterAfterShutdown()
{
    if (!shutdown_complete) return;
    Sys_flush_now();
    SaveXPRAM();
    MacClockSave(true);
    if (!AutomationLockDisplay()) {
        Serial.println("[POWER] Deep sleep deferred: a panel capture is still active");
        return;
    }
    shutdown_complete = false;
    SD_MMC.end();
    WiFi.mode(WIFI_OFF);
    AmacDisplayPrepareDeepSleep();
    enterDeepSleep();
    // A rejected sleep stays on the safe-to-power-off screen.
    if (!AmacDisplayRecoverFromSleepFailure()) {
        Serial.println("[POWER] Display recovery failed after rejected deep sleep");
        while (true) delay(1000);
    }
    AutomationUnlockDisplay();
}
#endif
