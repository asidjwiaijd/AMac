#if defined(BOARD_AMAC_P4)
#include <Arduino.h>
#include <WiFi.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "mac_clock.h"
#include "mac_network_time.h"

namespace {
constexpr uint64_t kMacEpoch = 2082844800ULL;
constexpr uint64_t kTaipeiOffset = 8 * 60 * 60;
constexpr int64_t kEarliestUtc = 1577836800; // 2020-01-01 UTC
constexpr int64_t kLatestUtc = UINT32_MAX - kMacEpoch - kTaipeiOffset;
portMUX_TYPE sample_lock = portMUX_INITIALIZER_UNLOCKED;
struct Sample { int64_t utc; uint32_t fraction; int64_t received_us; };
Sample pending_sample = {};
bool sample_pending = false;
bool initialized = false, online = false;
int64_t next_poll_us = 0, next_start_us = 0;
int64_t last_sync_us = 0, last_sync_utc = 0;
uint32_t sync_count = 0;
esp_err_t last_error = ESP_OK;

void receivedTime(struct timeval *tv) {
    if (!tv || tv->tv_sec < kEarliestUtc || tv->tv_sec > kLatestUtc ||
        tv->tv_usec < 0 || tv->tv_usec >= 1000000) return;
    const Sample sample = {int64_t(tv->tv_sec), uint32_t(tv->tv_usec), esp_timer_get_time()};
    portENTER_CRITICAL(&sample_lock);
    pending_sample = sample;
    sample_pending = true;
    portEXIT_CRITICAL(&sample_lock);
}
}

void MacNetworkTimePoll() {
    const int64_t now = esp_timer_get_time();
    Sample sample = {};
    portENTER_CRITICAL(&sample_lock);
    const bool received = sample_pending;
    if (received) { sample = pending_sample; sample_pending = false; }
    portEXIT_CRITICAL(&sample_lock);
    if (received && now >= sample.received_us) {
        const uint64_t elapsed = uint64_t(now - sample.received_us);
        const uint64_t local_us = (uint64_t(sample.utc) + kMacEpoch + kTaipeiOffset) *
            1000000ULL + sample.fraction + elapsed;
        if (local_us / 1000000 <= UINT32_MAX) {
            MacClockSetLocal(uint32_t(local_us / 1000000), uint32_t(local_us % 1000000));
            MacClockSave();
            last_sync_us = sample.received_us;
            last_sync_utc = sample.utc;
            ++sync_count;
            Serial.printf("[TIME] NTP synchronized: utc=%lld zone=Asia/Taipei syncs=%lu\n",
                (long long)last_sync_utc, (unsigned long)sync_count);
        }
    }
    if (now < next_poll_us) return;
    next_poll_us = now + 1000000;
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected) {
        online = false;
        next_start_us = 0;
        return;
    }
    if (online || now < next_start_us) return;
    if (!initialized) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,
            ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "time.cloudflare.com", "time.windows.com"));
        config.wait_for_sync = false;
        config.sync_cb = receivedTime;
        last_error = esp_netif_sntp_init(&config);
        initialized = last_error == ESP_OK;
    } else {
        last_error = esp_netif_sntp_start();
    }
    online = last_error == ESP_OK;
    next_start_us = now + 30000000;
    Serial.printf("[TIME] Network clock %s (err=%d)\n",
        online ? "started" : "retry pending", int(last_error));
}

void MacNetworkTimeDescribe(char *out, size_t capacity) {
    const uint32_t seconds = TimerDateTime();
    const time_t local_unix = int64_t(seconds) - int64_t(kMacEpoch);
    struct tm calendar = {};
    gmtime_r(&local_unix, &calendar); // Mac seconds already represent local wall time.
    char local[24];
    strftime(local, sizeof(local), "%Y-%m-%dT%H:%M:%S", &calendar);
    const long long age = sync_count ? (esp_timer_get_time() - last_sync_us) / 1000000 : -1;
    snprintf(out, capacity,
        "online=%u started=%u syncs=%lu utc=%lld age_s=%lld mac=%lu local=%s tz=Asia/Taipei error=%d",
        WiFi.status() == WL_CONNECTED, initialized, (unsigned long)sync_count,
        (long long)last_sync_utc, age, (unsigned long)seconds, local, int(last_error));
}
#endif
