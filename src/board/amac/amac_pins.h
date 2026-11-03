#pragma once

// Source: hardware/netlists/*2026-11-02*.tel, U3 module pad -> P4 GPIO.
// Internal C5 wiring: OSPTEK core-board c5-p4-connection.png.
#define AMAC_LCD_RESET_GPIO       23  // U3.32 -> R23 -> bottom FPC2.27
#define AMAC_LCD_BACKLIGHT_GPIO   13  // U3.28 -> bottom FPC2.2
#define AMAC_IR_GPIO              12  // U3.27 -> bottom FPC2.5 (input only)
#define AMAC_BATTERY_ADC_GPIO     22  // U3.31, R34=R35=100K, divide by 2
#define AMAC_CHARGE_STATUS_GPIO   21  // U3.30, U9.1, active low
#define AMAC_SD_CLK_GPIO          43
#define AMAC_SD_CMD_GPIO          44
#define AMAC_SD_D0_GPIO           39
#define AMAC_SD_D1_GPIO           40
#define AMAC_SD_D2_GPIO           41
#define AMAC_SD_D3_GPIO           42
#define AMAC_SD_DETECT_GPIO       45
#define AMAC_WIFI_CLK_GPIO        18
#define AMAC_WIFI_CMD_GPIO        19
#define AMAC_WIFI_D0_GPIO         14
#define AMAC_WIFI_D1_GPIO         15
#define AMAC_WIFI_D2_GPIO         16
#define AMAC_WIFI_D3_GPIO         17
#define AMAC_WIFI_RESET_GPIO      54

// The front board has an analog backlight dimmer. GPIO13 drives Q3 and
// the high-side LED supply switch. Default: HIGH enables the backlight.
// Firmware exposes on/off only; PWM would change an unqualified analog loop.
#ifndef AMAC_BACKLIGHT_ON_LEVEL
#define AMAC_BACKLIGHT_ON_LEVEL  1
#endif

// The supplied PDF contains no panel-specific GIP/gamma initialization or
// video porches. Use Espressif's 480x360 reference until qualified on hardware.
#ifndef AMAC_LCD_PIXEL_CLOCK_MHZ
#define AMAC_LCD_PIXEL_CLOCK_MHZ  15
#endif
#ifndef AMAC_LCD_LANE_RATE_MBPS
#define AMAC_LCD_LANE_RATE_MBPS   500
#endif
