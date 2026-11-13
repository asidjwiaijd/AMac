#pragma once
#include <stdint.h>

enum class LowBatteryAction { None, DeepSleep };

// All comparisons use measured pack voltage, independently of the SOC estimate.
class LowBatteryPolicy {
public:
    static constexpr uint16_t kDeepSleepMv = 4100;
    static constexpr uint32_t kConfirmMs = 5000;
    static constexpr uint64_t kCheckIntervalUs = 5000000ULL;

    LowBatteryAction observe(uint16_t mv, uint32_t now, bool charging = false) {
        LowBatteryAction next = LowBatteryAction::None;
        if (!charging && mv >= 2500 && mv <= 4500) {
            if (mv < kDeepSleepMv) next = LowBatteryAction::DeepSleep;
        }
        if (next != candidate) {
            candidate = next;
            candidate_since = now;
        }
        if (next == LowBatteryAction::None || now - candidate_since < kConfirmMs)
            return LowBatteryAction::None;
        return next;
    }

private:
    LowBatteryAction candidate = LowBatteryAction::None;
    uint32_t candidate_since = 0;
};

#if defined(BOARD_AMAC_P4)
// Access the flag only with __atomic_* operations. No ADC work runs on the CPU core.
extern bool low_battery_control_pending;
void LowBatteryBootCheck();                     // before LCD / SD / Wi-Fi startup
void LowBatteryPoll();                          // input task, once per second
void LowBatteryControlPoll();                   // CPU boundary
bool LowBatteryDeepSleepRequested();
uint16_t LowBatteryVoltageMv();                 // last valid ADC sample, 0 if invalid
void LowBatteryGuestIdle();                     // guest event-loop context
void LowBatteryGuestShutdown();                 // actual Mac PowerOff hook
void LowBatteryEnterAfterShutdown();            // after all emulator tasks stop
bool LowBatteryPrepareMaintenance();            // one shutdown, expires after 10 min
const char *LowBatteryWakeReason();             // cold, charge, sleep_failed
uint32_t LowBatteryChargeChecks();              // RTC checks before this boot
#endif
