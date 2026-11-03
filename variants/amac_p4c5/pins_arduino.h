#pragma once
#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t TX = 37;
static const uint8_t RX = 38;
static const uint8_t SDA = 7;
static const uint8_t SCL = 8;
static const uint8_t SS = 26;
static const uint8_t MOSI = 32;
static const uint8_t MISO = 33;
static const uint8_t SCK = 36;

static const uint8_t A0 = 16, A1 = 17, A2 = 18, A3 = 19;
static const uint8_t A4 = 20, A5 = 21, A6 = 22, A7 = 23;
static const uint8_t A8 = 49, A9 = 50, A10 = 51, A11 = 52, A12 = 53, A13 = 54;

// SD slot 0 uses fixed IOMUX pins 39..44. Card power is the fixed 3V3 rail.
// GPIO45 is the socket's detection contact; DO NOT define a POWER_PIN.
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT 0
#define BOARD_SDMMC_POWER_CHANNEL -1

// Internal OSPTEK P4-C5 interconnect. No dependency on external GPIO7..10.
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK   18
#define BOARD_SDIO_ESP_HOSTED_CMD   19
#define BOARD_SDIO_ESP_HOSTED_D0    14
#define BOARD_SDIO_ESP_HOSTED_D1    15
#define BOARD_SDIO_ESP_HOSTED_D2    16
#define BOARD_SDIO_ESP_HOSTED_D3    17
#define BOARD_SDIO_ESP_HOSTED_RESET 54
