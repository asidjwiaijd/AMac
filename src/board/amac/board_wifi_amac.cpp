#include "board_config.h"
#include <WiFi.h>

extern "C" void AmacWifiPrepare(void)
{
    static bool prepared = false;
    if (prepared) return;
    prepared = true;
    WiFi.setPins(AMAC_WIFI_CLK_GPIO, AMAC_WIFI_CMD_GPIO, AMAC_WIFI_D0_GPIO,
                 AMAC_WIFI_D1_GPIO, AMAC_WIFI_D2_GPIO, AMAC_WIFI_D3_GPIO,
                 AMAC_WIFI_RESET_GPIO);
}
