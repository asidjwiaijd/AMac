// Arduino's precompiled ESP-Hosted 2.12.3 transport expects an ESP32-C6 and
// asserts on C5. Rebuild that translation unit from the matching upstream
// source with the actual co-processor target. Other boards use the SDK archive.
#include "sdkconfig.h"
#include "esp_hosted_host_fw_ver.h"
#if ESP_HOSTED_VERSION_MAJOR_1 != 2 || ESP_HOSTED_VERSION_MINOR_1 != 12 || ESP_HOSTED_VERSION_PATCH_1 != 3
#error "Review the aMac C5 transport override when changing ESP-Hosted versions"
#endif

#undef CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6
#define CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5 1
#include "../../../third_party/esp_hosted_2_12_3/transport_drv.c"

#if !H_SLAVE_TARGET_ESP32C5 || H_SLAVE_TARGET_ESP32C6
#error "aMac transport must validate the ESP32-C5, not the SDK's ESP32-C6 default"
#endif
