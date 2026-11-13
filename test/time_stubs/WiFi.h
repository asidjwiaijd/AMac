#pragma once
constexpr int WL_CONNECTED = 3;
inline int test_wifi_status = 0;
inline struct { int status() { return test_wifi_status; } } WiFi;
