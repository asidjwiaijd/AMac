#pragma once
constexpr int ESP_RST_POWERON = 1, ESP_RST_DEEPSLEEP = 8;
inline int test_reset_reason = ESP_RST_POWERON;
inline int esp_reset_reason() { return test_reset_reason; }
