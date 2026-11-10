#include "board_power.h"
#include "board_config.h"
#include "board_display.h"
#include <Arduino.h>

extern "C" bool BoardPowerHW_Describe(BoardBatteryDescription *out)
{
#if !defined(AMAC_ENABLE_BATTERY_ESTIMATE) || !AMAC_ENABLE_BATTERY_ESTIMATE
    // The netlists give a voltage divider and CHRG only. Leave the guest
    // battery service off until pack type/calibration are confirmed.
    (void)out;
    return false;
#else
    if (!out) return false;
    // No current sensor or pack capacity is provided. Do not invent time estimates.
    *out = {1, 0, 0, 1000};
    return true;
#endif
}
extern "C" bool BoardPowerHW_Read(BoardBatteryRaw *out)
{
    if (!out) return false;
    // Automatic low-voltage protection also reads raw voltage when the
    // optional guest percentage estimate is disabled.
    static bool configured = false;
    if (!configured) {
        pinMode(AMAC_CHARGE_STATUS_GPIO, INPUT);
        // Attach the channel before configuring its attenuation. Arduino
        // otherwise rejects analogSetPinAttenuation on an unused pin.
        (void)analogReadMilliVolts(AMAC_BATTERY_ADC_GPIO);
        analogSetPinAttenuation(AMAC_BATTERY_ADC_GPIO, ADC_11db);
        configured = true;
    }
    uint32_t mv = 0;
    for (int i = 0; i < 8; ++i) mv += analogReadMilliVolts(AMAC_BATTERY_ADC_GPIO);
    *out = {};
    out->pack_mv = (mv / 8) * 2; // R34/R35: 100K / 100K, ADC1 channel 6.
    out->present = out->pack_mv >= 2500 && out->pack_mv <= 4500;
    out->charging = out->present && digitalRead(AMAC_CHARGE_STATUS_GPIO) == LOW;
    // Voltage-only estimate: full/idle USB power cannot be distinguished.
    out->charger_known = true;
    out->charger_connected = out->charging;
    return true;
}
extern "C" bool BoardPowerHW_SetBacklight(int percent)
{
    BoardDisplay_SetBacklight(percent);
    return true;
}
extern "C" int BoardPowerHW_InitialBacklight(void) { return 100; }
extern "C" bool BoardPowerHW_SetCharging(bool) { return false; }
