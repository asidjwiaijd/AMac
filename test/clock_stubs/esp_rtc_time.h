#pragma once
#include <stdint.h>
inline uint64_t test_rtc_us = 1000000;
inline uint64_t esp_rtc_get_time_us() { return test_rtc_us; }
