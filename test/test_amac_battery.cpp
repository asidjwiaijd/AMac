#include "low_battery.h"
#include "amac_charge_wake.h"
#include "board_power.h"
#include "board_config.h"
#include "amac/amac_battery_telemetry.h"
#include <Arduino.h>
#include <cassert>
#include <cstdio>

uint32_t g_fake_millis = 0;
FakeSerial Serial;
static unsigned reads, configured;
static uint32_t adc_mv = 1900;
static bool charging;

void pinMode(int pin, int mode) { assert(pin == 21 && mode == INPUT); }
void analogSetPinAttenuation(int pin, int attenuation)
{
    assert(pin == 22 && attenuation == ADC_11db);
    ++configured;
}
uint32_t analogReadMilliVolts(int pin) { assert(pin == 22); ++reads; return adc_mv; }
int digitalRead(int pin) { assert(pin == 21); return charging ? LOW : HIGH; }
extern "C" void BoardDisplay_SetBacklight(int) {}

int main()
{
    using Action = LowBatteryAction;
    static_assert(LowBatteryPolicy::kDeepSleepMv == 4100);
    static_assert(LowBatteryPolicy::kCheckIntervalUs == 5000000ULL);
    LowBatteryPolicy policy;
    assert(policy.observe(4100, 0) == Action::None);
    assert(policy.observe(4099, 1000) == Action::None);
    assert(policy.observe(4099, 5999) == Action::None);
    assert(policy.observe(4099, 6000) == Action::DeepSleep);
    assert(policy.observe(3800, 7000, true) == Action::None);
    assert(policy.observe(3500, 12000, true) == Action::None);
    assert(policy.observe(4099, 13000) == Action::None);
    assert(policy.observe(4099, 18000) == Action::DeepSleep);
    assert(policy.observe(0, 19000) == Action::None);
    assert(policy.observe(4099, 20000) == Action::None);
    assert(policy.observe(4501, 24000) == Action::None);
    assert(policy.observe(4099, 25000) == Action::None);
    assert(policy.observe(4100, 30000) == Action::None);
    assert(policy.observe(4099, 31000) == Action::None);
    assert(policy.observe(4099, 36000) == Action::DeepSleep);
    LowBatteryPolicy wrap;
    assert(wrap.observe(4099, UINT32_MAX - 1999) == Action::None);
    assert(wrap.observe(4099, 2999) == Action::None);
    assert(wrap.observe(4099, 3000) == Action::DeepSleep);
    AmacChargeWakeState wait;
    assert(!wait.observe(true));
    wait.begin(false);
    assert(!wait.observe(false) && wait.observe(true));
    wait.begin(true);
    assert(!wait.observe(true)); // Already charging at shutdown cannot wake it.
    assert(!wait.observe(false));
    assert(wait.observe(true));

    // The optional SOC estimate stays off. Raw protection still initializes ADC.
    BoardBatteryDescription desc = {};
    assert(!BoardPowerHW_Describe(&desc));
    assert(!BoardPowerHW_Read(nullptr));
    AmacBatteryTelemetry telemetry;
    assert(!AmacBatteryGetTelemetry(nullptr));
    assert(!AmacBatteryGetTelemetry(&telemetry));
    assert(!telemetry.voltage_valid && telemetry.charging == -1 && telemetry.chrg_low == -1);
    BoardBatteryRaw raw = {};
    assert(BoardPowerHW_Read(&raw));
    assert(reads == 9 && configured == 1 && raw.present && raw.pack_mv == 3800);
    assert(!raw.charging);
    assert(AmacBatteryGetTelemetry(&telemetry));
    assert(telemetry.samples == 1 && telemetry.voltage_valid && telemetry.charging == -1);
    g_fake_millis = 999;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.charging == -1); // Repeated fast polls cannot bypass debounce.
    g_fake_millis = 1000;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.charging == 0 && telemetry.chrg_low == 0);
    charging = true;
    adc_mv = 1850;
    g_fake_millis = 1500;
    assert(BoardPowerHW_Read(&raw));
    assert(reads == 33 && configured == 1 && raw.pack_mv == 3700 && raw.charging);
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.chrg_low == 1 && telemetry.charging == 0);
    charging = false; // A short glitch must not announce charging.
    g_fake_millis = 2000;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.charging == 0);
    charging = true;
    g_fake_millis = 2200;
    assert(BoardPowerHW_Read(&raw));
    g_fake_millis = 3200;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.charging == 1);
    const unsigned before_cache = reads;
    g_fake_millis = 8200;
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.voltage_valid && telemetry.charging == 1);
    g_fake_millis = 8201;
    AmacBatteryGetTelemetry(&telemetry);
    assert(!telemetry.voltage_valid && telemetry.charging == -1 && telemetry.chrg_low == -1);
    assert(reads == before_cache); // Diagnostics never reads ADC on another task.
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.voltage_valid && telemetry.charging == -1); // Gap needs new debounce.
    g_fake_millis = 9201;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.charging == 1);
    adc_mv = 0;
    assert(BoardPowerHW_Read(&raw) && !raw.present && !raw.charging);
    AmacBatteryGetTelemetry(&telemetry);
    assert(!telemetry.voltage_valid && telemetry.charging == -1 && telemetry.chrg_low == 1);
    adc_mv = 2100;
    g_fake_millis = UINT32_MAX - 499;
    assert(BoardPowerHW_Read(&raw));
    g_fake_millis = 500;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.pack_mv == 4200 && telemetry.charging == 1); // millis wrap.
    charging = false;
    g_fake_millis = 600;
    assert(BoardPowerHW_Read(&raw));
    g_fake_millis = 1600;
    assert(BoardPowerHW_Read(&raw));
    AmacBatteryGetTelemetry(&telemetry);
    assert(telemetry.charging == 0 && telemetry.chrg_low == 0);
    std::puts("aMac battery thresholds, ADC, charge debounce, stale data and wraparound passed");
}
