#include "low_battery.h"
#include "board_power.h"
#include "board_config.h"
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
    LowBatteryPolicy policy;
    assert(policy.observe(3900, 0) == Action::None);
    assert(policy.observe(3899, 1000) == Action::None);
    assert(policy.observe(3899, 5999) == Action::None);
    assert(policy.observe(3899, 6000) == Action::Standby);
    policy.noteActivity(6000);
    assert(policy.observe(3800, 65000) == Action::None);
    assert(policy.observe(3800, 66000) == Action::Standby);
    assert(policy.observe(3700, 67000) == Action::Standby); // strict deep threshold
    assert(policy.observe(3699, 68000) == Action::None);
    policy.noteActivity(69000);
    assert(policy.observe(3699, 73000) == Action::DeepSleep); // critical overrides wake grace
    assert(policy.observe(3900, 74000) == Action::None);
    assert(policy.observe(3699, 75000) == Action::None);
    assert(policy.observe(0, 79000) == Action::None);
    assert(policy.observe(3699, 80000) == Action::None);
    assert(policy.observe(4501, 84000) == Action::None);
    assert(policy.observe(3699, 85000) == Action::None);
    assert(policy.observe(3699, 90000) == Action::DeepSleep);

    LowBatteryPolicy wrap;
    assert(wrap.observe(3800, UINT32_MAX - 1999) == Action::None);
    assert(wrap.observe(3800, 3000) == Action::Standby);
    wrap.noteActivity(UINT32_MAX - 999);
    assert(wrap.observe(3800, 50000) == Action::None);
    assert(wrap.observe(3800, 59000) == Action::Standby);

    // The optional SOC estimate stays off. Raw protection still initializes ADC.
    BoardBatteryDescription desc = {};
    assert(!BoardPowerHW_Describe(&desc));
    assert(!BoardPowerHW_Read(nullptr));
    BoardBatteryRaw raw = {};
    assert(BoardPowerHW_Read(&raw));
    assert(reads == 9 && configured == 1 && raw.present && raw.pack_mv == 3800);
    assert(!raw.charging);
    charging = true;
    adc_mv = 1850;
    assert(BoardPowerHW_Read(&raw));
    assert(reads == 17 && configured == 1 && raw.pack_mv == 3700 && raw.charging);
    adc_mv = 0;
    assert(BoardPowerHW_Read(&raw) && !raw.present && !raw.charging);
    std::puts("aMac battery thresholds, debounce, wake grace and ADC path passed");
}
