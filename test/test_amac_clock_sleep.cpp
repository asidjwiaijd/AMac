// Simulate startup clearing ordinary RAM while RTC_DATA_ATTR survives.
#include "../src/basilisk/mac_clock_esp32.cpp"
#include <cassert>
#include <cstdio>

static void reboot(int reason, uint64_t elapsed_us)
{
    test_reset_reason = reason;
    test_rtc_us += elapsed_us;
    test_now_us = 100000;
    initialized = false;
    user_set = dirty = save_attempted = false;
}

int main()
{
    for (unsigned reg = 0; reg < 4; ++reg) TimerWriteRTCByte(reg, uint8_t(1234567890u >> (reg * 8)));
    MacClockSave(true);
    assert(test_commits == 1);
    test_now_us += 2000000;
    test_rtc_us += 2000000;
    MacClockPrepareDeepSleep();
    assert(TimerDateTime() == 1234567892);
    reboot(ESP_RST_DEEPSLEEP, 300000000);
    assert(TimerDateTime() == 1234568192);
    for (int i = 0; i < 12; ++i) {
        MacClockPrepareDeepSleep();
        reboot(ESP_RST_DEEPSLEEP, 300000000);
    }
    assert(TimerDateTime() == 1234571792);
    // Short ADC-check boots must keep subsecond time across repeated sleeps.
    for (int i = 0; i < 100; ++i) {
        test_now_us += 250000;
        test_rtc_us += 250000;
        MacClockPrepareDeepSleep();
        reboot(ESP_RST_DEEPSLEEP, 300250000);
    }
    assert(TimerDateTime() == 1234601842);
    assert(test_commits == 1); // RTC voltage checks do not wear NVS.
    reboot(ESP_RST_POWERON, 0);
    assert(TimerDateTime() == 1234567890); // Cold boots use the persisted checkpoint.
    sleep_clock.magic = 0;
    reboot(ESP_RST_DEEPSLEEP, 300000000);
    assert(TimerDateTime() == 1234567890);
    test_saved = 0;
    reboot(ESP_RST_DEEPSLEEP, 300000000);
    assert(TimerDateTime() == 0); // Historical dates keep their existing meaning.
    std::puts("aMac RTC clock across repeated deep sleep and cold reset passed");
}
