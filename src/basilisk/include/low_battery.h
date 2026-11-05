#pragma once
#include <stdint.h>

enum class LowBatteryAction { None, Standby, DeepSleep };

// All comparisons use measured pack voltage, independently of the SOC estimate.
class LowBatteryPolicy {
public:
    static constexpr uint16_t kStandbyMv = 3900;
    static constexpr uint16_t kDeepSleepMv = 3700;
    static constexpr uint16_t kResumeMv = 3900;
    static constexpr uint32_t kConfirmMs = 5000;
    static constexpr uint32_t kWakeGraceMs = 60000;
    static constexpr uint64_t kCheckIntervalUs = 300000000ULL;

    LowBatteryAction observe(uint16_t mv, uint32_t now) {
        LowBatteryAction next = LowBatteryAction::None;
        if (mv >= 2500 && mv <= 4500) {
            if (mv < kDeepSleepMv) next = LowBatteryAction::DeepSleep;
            else if (mv < kStandbyMv) next = LowBatteryAction::Standby;
        }
        if (next != candidate) {
            candidate = next;
            candidate_since = now;
        }
        if (next == LowBatteryAction::None || now - candidate_since < kConfirmMs)
            return LowBatteryAction::None;
        if (next == LowBatteryAction::Standby && had_activity && now - last_activity < kWakeGraceMs)
            return LowBatteryAction::None;
        return next;
    }

    void noteActivity(uint32_t now) {
        last_activity = now;
        had_activity = true;
    }

private:
    LowBatteryAction candidate = LowBatteryAction::None;
    uint32_t candidate_since = 0;
    uint32_t last_activity = 0;
    bool had_activity = false;
};

#if defined(BOARD_AMAC_P4)
// Access the flag only with __atomic_* operations. No ADC work runs on the CPU core.
extern bool low_battery_control_pending;
void LowBatteryBootCheck();                     // before LCD / SD / Wi-Fi startup
void LowBatteryPoll();                          // input task, once per second
void LowBatteryNoteActivity();                  // any input task
void LowBatteryControlPoll();                   // CPU boundary, standby only
bool LowBatteryDeepSleepRequested();
uint16_t LowBatteryVoltageMv();                 // last valid ADC sample, 0 if invalid
void LowBatteryGuestIdle();                     // guest event-loop context
void LowBatteryGuestShutdown();                 // actual Mac PowerOff hook
void LowBatteryEnterAfterShutdown();            // after all emulator tasks stop
#endif
