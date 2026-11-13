#pragma once
#include <stdint.h>

// RTC-retained state. A charger already active at shutdown must first stop
// before a new active signal may wake the system. No voltage-rebound wake.
struct AmacChargeWakeState {
    bool active = false;
    bool armed = false;
    uint32_t checks = 0;

    void begin(bool charging) {
        active = true;
        armed = !charging;
        checks = 0;
    }
    bool observe(bool charging) {
        if (!charging) armed = true;
        return active && armed && charging;
    }
};
