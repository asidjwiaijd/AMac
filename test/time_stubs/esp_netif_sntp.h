#pragma once
#include <sys/time.h>
#include <stddef.h>
#include <initializer_list>
#include "nvs.h"
using esp_err_t = int;
struct esp_sntp_config_t {
    bool wait_for_sync, start;
    void (*sync_cb)(struct timeval *);
    size_t num_of_servers;
    const char *servers[3];
};
inline esp_sntp_config_t testConfig(size_t count, std::initializer_list<const char *> servers) {
    esp_sntp_config_t result = {true, true, nullptr, count, {}};
    size_t i = 0;
    for (const char *name : servers) result.servers[i++] = name;
    return result;
}
#define ESP_SNTP_SERVER_LIST(...) {__VA_ARGS__}
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(count, list) testConfig(count, list)
inline unsigned test_init_calls = 0, test_start_calls = 0;
inline esp_err_t test_init_result = ESP_OK, test_start_result = ESP_OK;
inline esp_sntp_config_t test_sntp_config = {};
inline esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config) {
    ++test_init_calls;
    test_sntp_config = *config;
    return test_init_result;
}
inline esp_err_t esp_netif_sntp_start() { ++test_start_calls; return test_start_result; }
