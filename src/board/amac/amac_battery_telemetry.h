#pragma once

#include <stdint.h>

// Physical readings, independent of the optional guest percentage estimate.
// CHRG alone cannot distinguish a full battery from an unplugged charger.
struct AmacBatteryTelemetry {
    uint16_t pack_mv = 0;
    bool voltage_valid = false;
    int8_t charging = -1; // -1 unknown, 0 not charging, 1 charging; 1 s debounce
    int8_t chrg_low = -1; // instantaneous GPIO21; -1 when no fresh sample
    uint32_t sample_ms = 0;
    uint32_t samples = 0;
};

// Cached copy only; returns false before sampling. Older than 5 s is invalid.
bool AmacBatteryGetTelemetry(AmacBatteryTelemetry *out);
