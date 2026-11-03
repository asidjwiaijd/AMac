# ST7701 MIPI driver

Copied from Espressif esp-iot-solution commit
`8f2257f7e88aeba4c325fe288a5121b92900391c` (component 2.0.3).

Source: https://github.com/espressif/esp-iot-solution/tree/8f2257f7e88aeba4c325fe288a5121b92900391c/components/display/lcd/esp_lcd_st7701

Only the MIPI backend and its headers are included. aMac calls
`esp_lcd_new_panel_st7701_mipi` directly; the RGB backend and its SPI
dependencies are unnecessary. The reference initialization and timing are
Espressif's 480x360 configuration, not qualified HD20034 manufacturer data.
The original Apache 2.0 license is in LICENSE.

The 480x360 reference macro is named 60HZ upstream, but its 15 MHz / 520 / 390
timing evaluates to about 73.96 Hz. aMac uses those values as a reference,
pending panel-specific timing data and hardware qualification.
