#include "mac_network_time.h"
#include "mac_clock.h"
#include "WiFi.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <cstdio>

static void pollAfter(int64_t elapsed) { test_now_us += elapsed; MacNetworkTimePoll(); }
static void sample(int64_t utc, long fraction = 0) {
    struct timeval tv = {time_t(utc), fraction};
    test_sntp_config.sync_cb(&tv);
}

int main() {
    MacClockSetLocal(42);
    MacNetworkTimePoll();
    assert(test_init_calls == 0 && test_commits == 0);
    test_wifi_status = WL_CONNECTED;
    test_init_result = -1;
    pollAfter(1000000);
    assert(test_init_calls == 1);
    pollAfter(1000000);
    assert(test_init_calls == 1); // Failed init is bounded, no busy retry.
    test_init_result = ESP_OK;
    pollAfter(30000000);
    assert(test_init_calls == 2 && test_sntp_config.start);
    assert(!test_sntp_config.wait_for_sync && test_sntp_config.num_of_servers == 3);
    assert(!strcmp(test_sntp_config.servers[0], "ntp.aliyun.com"));
    assert(!strcmp(test_sntp_config.servers[1], "time.cloudflare.com"));
    assert(!strcmp(test_sntp_config.servers[2], "time.windows.com"));
    const uint32_t before = TimerDateTime();
    const int64_t utc = 1794549084; // 2026-11-13T05:51:24Z
    const uint32_t local = uint32_t(utc + 2082844800LL + 28800);
    sample(utc, 250000);
    assert(TimerDateTime() == before && test_commits == 0); // Callback cannot touch clock/NVS.
    pollAfter(1250000);
    assert(TimerDateTime() == local + 1 && test_saved == local + 1 && test_commits == 1);
    pollAfter(500000);
    assert(TimerDateTime() == local + 2); // Fraction and queue latency preserved.
    sample(0);
    sample(2212093696LL);
    sample(utc, -1);
    sample(utc, 1000000);
    MacNetworkTimePoll();
    assert(TimerDateTime() == local + 2 && test_commits == 1);
    char state[300];
    MacNetworkTimeDescribe(state, sizeof(state));
    assert(strstr(state, "syncs=1") && strstr(state, "tz=Asia/Taipei"));
    assert(strstr(state, "local=2026-11-13T13:51:26"));

    test_wifi_status = 0;
    pollAfter(1000000);
    const uint32_t offline = TimerDateTime();
    pollAfter(60000000);
    assert(TimerDateTime() == offline + 60 && test_init_calls == 2 && test_start_calls == 0);
    MacNetworkTimeDescribe(state, sizeof(state));
    assert(strstr(state, "online=0") && strstr(state, "syncs=1"));
    test_wifi_status = WL_CONNECTED;
    test_start_result = -1;
    pollAfter(1000000);
    assert(test_start_calls == 1);
    pollAfter(1000000);
    assert(test_start_calls == 1);
    test_start_result = ESP_OK;
    pollAfter(30000000);
    assert(test_start_calls == 2 && test_init_calls == 2);
    sample(utc + 7200);
    MacNetworkTimePoll();
    assert(TimerDateTime() == local + 7200 && test_saved == local + 7200 && test_commits == 2);
    MacNetworkTimeDescribe(state, sizeof(state));
    assert(strstr(state, "syncs=2") && strstr(state, "online=1"));
    std::puts("PASS: asynchronous NTP, Taipei time, persistence, invalid samples, offline clock and reconnect retries");
}
