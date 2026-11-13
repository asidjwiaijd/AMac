// Production shutdown/boot code is inserted below by test_amac_shutdown.py.
#include "low_battery.h"
#include "amac_charge_wake.h"
#include "board_power.h"
#include "amac/amac_pins.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#define RTC_DATA_ATTR
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_RST_DEEPSLEEP=8, OUTPUT=1, LOW=0;
constexpr int GPIO_NUM_13=13, GPIO_NUM_23=23, GPIO_NUM_54=54;
constexpr int RTC_GPIO_MODE_OUTPUT_ONLY=1, WIFI_OFF=0;
static uint32_t now_ms;
static int reset_reason, timer_error, sleep_error;
static uint64_t wake_timer;
static bool success_sleep=true, lock_ok=true, has_mac=true, recovering=true;
static bool holds[55];
static unsigned release54, shutdown_calls, reads, sd_ends, display_off, recovered, unlocks;
static BoardBatteryRaw battery;
static std::deque<BoardBatteryRaw> read_script;
static std::vector<std::string> trace;
struct Sleeping {};
static uint32_t millis() { return now_ms; }
static void delay(uint32_t ms) { now_ms+=ms; }
struct { template<class... T> void printf(const char *,T...) {} void println(const char *) {} void flush() {} } Serial;
static int esp_reset_reason() { return reset_reason; }
static const char *esp_err_to_name(int) { return "test_error"; }
static int esp_sleep_enable_timer_wakeup(uint64_t us) { wake_timer=us;return timer_error; }
static int esp_deep_sleep_try_to_start() { trace.push_back("sleep");if(success_sleep) throw Sleeping{};return sleep_error; }
static void gpio_hold_dis(int pin) { holds[pin]=false;if(pin==54) ++release54; }
static void gpio_hold_en(int pin) { holds[pin]=true; }
static void rtc_gpio_hold_dis(int pin) { holds[pin]=false; }
static void rtc_gpio_deinit(int) {}
static void rtc_gpio_init(int) {}
static void rtc_gpio_set_direction(int,int) {}
static void rtc_gpio_set_level(int,int) {}
static void rtc_gpio_hold_en(int pin) { holds[pin]=true; }
static void pinMode(int,int) {}
static void digitalWrite(int,int value) { assert(value==LOW); }
extern "C" bool BoardPowerHW_Read(BoardBatteryRaw *out) {
    ++reads;
    if(!read_script.empty()) { *out=read_script.front();read_script.pop_front(); }
    else *out=battery;
    return true;
}
static void MacClockPrepareDeepSleep() { trace.push_back("clock"); }
static void MacClockSave(bool) { trace.push_back("save_clock"); }
void Sys_flush_now() { trace.push_back("flush"); }
void SaveXPRAM() { trace.push_back("xpram"); }
static bool AutomationLockDisplay() { return lock_ok; }
static void AutomationUnlockDisplay() { ++unlocks; }
static bool HasMacStarted() { return has_mac; }
static int16_t MacRequestShutdown() { ++shutdown_calls;return 0; }
static void BirthdayGift_Close() {}
extern "C" void AmacDisplayPrepareDeepSleep() { ++display_off;trace.push_back("display_off"); }
extern "C" bool AmacDisplayRecoverFromSleepFailure() { ++recovered;return recovering; }
struct { void end() { ++sd_ends;trace.push_back("sd_end"); } } SD_MMC;
struct { void mode(int) { trace.push_back("wifi_off"); } } WiFi;

// INSERT_PRODUCTION

static void reset()
{
    now_ms=1000;reset_reason=0;timer_error=sleep_error=0;wake_timer=0;
    success_sleep=lock_ok=has_mac=recovering=true;
    for(bool &hold:holds) hold=false;
    release54=shutdown_calls=reads=sd_ends=display_off=recovered=unlocks=0;
    battery={};battery.present=true;battery.pack_mv=4200;
    read_script.clear();trace.clear();
    charger_wait={};policy={};pending=LowBatteryAction::None;
    shutdown_requested=shutdown_complete=automatic_shutdown=poweroff_in_progress=maintenance_hold=sampled=false;
    maintenance_since=shutdown_at=last_sample=last_shutdown_attempt=0;pack_mv=0;
    wake_reason="cold";charge_checks=0;low_battery_control_pending=false;
}
template<class F> static void slept(F action)
{
    bool sleeping=false;
    try { action(); } catch(const Sleeping &) { sleeping=true; }
    assert(sleeping && wake_timer==5000000ULL);
    assert(holds[13] && holds[23] && holds[54]);
}

int main()
{
    reset();battery.pack_mv=4098;
    LowBatteryPoll();assert(!LowBatteryDeepSleepRequested());
    now_ms=5999;LowBatteryPoll();assert(!LowBatteryDeepSleepRequested());
    now_ms=7000;LowBatteryPoll();assert(LowBatteryDeepSleepRequested());
    has_mac=false;LowBatteryGuestIdle();assert(shutdown_calls==0);
    has_mac=true;LowBatteryGuestIdle();assert(shutdown_calls==1);
    LowBatteryEnterAfterShutdown();assert(sd_ends==0); // A save/cancel dialog is not PowerOff.
    LowBatteryGuestShutdown();
    slept([]{LowBatteryEnterAfterShutdown();});
    assert(sd_ends==1 && display_off==1 && charger_wait.armed);
    assert(trace==std::vector<std::string>({"flush","xpram","save_clock","sd_end","wifi_off","display_off","clock","sleep"}));

    // A rebounding voltage cannot start Mac. RTC checks keep peripheral holds.
    reset_reason=ESP_RST_DEEPSLEEP;battery.pack_mv=4200;release54=0;
    slept([]{LowBatteryBootCheck();});
    assert(release54==0 && charger_wait.checks==1 && sd_ends==1);
    battery.charging=true;battery.pack_mv=3800;
    LowBatteryBootCheck();
    assert(std::string(LowBatteryWakeReason())=="charge" && LowBatteryChargeChecks()==2);
    assert(!holds[13] && !holds[23] && !holds[54]);
    // Ordinary RAM is reinitialized by a real deep-sleep boot.
    sampled=false;pending=LowBatteryAction::None;shutdown_requested=false;
    LowBatteryPoll();assert(!LowBatteryDeepSleepRequested());

    // Manual shutdown: one maintenance window, then wait for a new charge cycle.
    reset();battery.charging=true;
    LowBatteryGuestShutdown();LowBatteryEnterAfterShutdown();assert(sd_ends==0);
    now_ms+=9999;LowBatteryEnterAfterShutdown();assert(sd_ends==0);
    ++now_ms;slept([]{LowBatteryEnterAfterShutdown();});assert(!charger_wait.armed);
    reset_reason=ESP_RST_DEEPSLEEP;
    slept([]{LowBatteryBootCheck();});assert(!charger_wait.armed);
    battery.present=false;battery.charging=false;
    slept([]{LowBatteryBootCheck();});assert(!charger_wait.armed); // Invalid sample cannot rearm.
    battery.present=true;
    slept([]{LowBatteryBootCheck();});assert(charger_wait.armed);
    battery.charging=true;
    LowBatteryBootCheck();assert(std::string(LowBatteryWakeReason())=="charge");

    reset();battery.present=false;LowBatteryGuestShutdown();now_ms+=10000;
    slept([]{LowBatteryEnterAfterShutdown();});assert(!charger_wait.armed);

    // A transient low CHRG at the first read is rejected by confirmation.
    reset();charger_wait.begin(false);reset_reason=ESP_RST_DEEPSLEEP;
    BoardBatteryRaw pulse=battery;pulse.charging=true;
    read_script={pulse,battery};
    slept([]{LowBatteryBootCheck();});

    reset();battery.pack_mv=4098;
    slept([]{LowBatteryBootCheck();});assert(now_ms==6000 && sd_ends==0);
    reset();battery.pack_mv=3800;battery.charging=true;
    LowBatteryBootCheck();assert(now_ms==1000 && sd_ends==0);
    reset();battery.pack_mv=4100;
    LowBatteryBootCheck();assert(now_ms==1000); // Strictly below threshold.

    reset();assert(LowBatteryPrepareMaintenance());
    LowBatteryGuestShutdown();now_ms+=20000;
    LowBatteryEnterAfterShutdown();assert(sd_ends==0 && !shutdown_complete);
    reset();assert(LowBatteryPrepareMaintenance());now_ms+=600000;
    LowBatteryGuestShutdown();now_ms+=10000;
    slept([]{LowBatteryEnterAfterShutdown();}); // Expired preparation cannot keep it awake.
    reset();LowBatteryGuestShutdown();assert(LowBatteryPrepareMaintenance());
    now_ms+=20000;LowBatteryEnterAfterShutdown();assert(sd_ends==0);
    reset();assert(LowBatteryPrepareMaintenance());shutdown_requested=true;
    LowBatteryGuestShutdown();
    slept([]{LowBatteryEnterAfterShutdown();}); // Low voltage overrides a maintenance hold.

    reset();LowBatteryGuestShutdown();now_ms+=10000;lock_ok=false;
    LowBatteryEnterAfterShutdown();assert(sd_ends==0 && shutdown_complete && !poweroff_in_progress);
    lock_ok=true;slept([]{LowBatteryEnterAfterShutdown();});
    assert(!LowBatteryPrepareMaintenance()); // Too late once teardown starts.

    reset();LowBatteryGuestShutdown();now_ms+=10000;success_sleep=false;sleep_error=42;
    LowBatteryEnterAfterShutdown();
    assert(std::string(LowBatteryWakeReason())=="sleep_failed" && !charger_wait.active);
    assert(!holds[13] && !holds[23] && !holds[54] && recovered==1 && unlocks==1 && !poweroff_in_progress);
    reset();charger_wait.begin(false);reset_reason=ESP_RST_DEEPSLEEP;timer_error=43;
    LowBatteryBootCheck();
    assert(!charger_wait.active && std::string(LowBatteryWakeReason())=="sleep_failed");
    std::puts("PASS: 4.1 V shutdown, charging wake, RTC holds, save safety, maintenance and sleep failure recovery");
}
