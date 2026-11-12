#include "board_power.h"
#include "board_config.h"
#include "board_display.h"
#include "amac_battery_telemetry.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>

namespace {
portMUX_TYPE telemetry_lock = portMUX_INITIALIZER_UNLOCKED;
AmacBatteryTelemetry telemetry;
int8_t charge_candidate = -1;
uint32_t candidate_since = 0;
}

bool AmacBatteryGetTelemetry(AmacBatteryTelemetry *out)
{
    if (!out) return false;
    portENTER_CRITICAL(&telemetry_lock);
    *out = telemetry;
    portEXIT_CRITICAL(&telemetry_lock);
    if (!out->samples || (uint32_t)(millis() - out->sample_ms) > 5000) {
        out->voltage_valid = false;
        out->charging = out->chrg_low = -1;
    }
    return out->samples != 0;
}

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
    const bool chrg_low = digitalRead(AMAC_CHARGE_STATUS_GPIO) == LOW;
    out->charging = out->present && chrg_low;
    // Voltage-only estimate: full/idle USB power cannot be distinguished.
    out->charger_known = true;
    out->charger_connected = out->charging;
    const uint32_t now = millis();
    portENTER_CRITICAL(&telemetry_lock);
    // A sampling gap cannot establish a stable signal across that gap.
    if (!telemetry.samples || (uint32_t)(now - telemetry.sample_ms) > 5000) {
        charge_candidate = -1;
        telemetry.charging = -1;
    }
    telemetry.pack_mv = out->pack_mv;
    telemetry.voltage_valid = out->present;
    telemetry.chrg_low = chrg_low ? 1 : 0;
    telemetry.sample_ms = now;
    if (telemetry.samples != UINT32_MAX) ++telemetry.samples;
    if (!out->present) {
        charge_candidate = telemetry.charging = -1;
    } else if (charge_candidate != telemetry.chrg_low) {
        charge_candidate = telemetry.chrg_low;
        candidate_since = now;
    } else if ((uint32_t)(now - candidate_since) >= 1000) {
        telemetry.charging = charge_candidate;
    }
    portEXIT_CRITICAL(&telemetry_lock);
    return true;
}
extern "C" bool BoardPowerHW_SetBacklight(int percent)
{
    BoardDisplay_SetBacklight(percent);
    return true;
}
extern "C" int BoardPowerHW_InitialBacklight(void) { return 100; }
extern "C" bool BoardPowerHW_SetCharging(bool) { return false; }
