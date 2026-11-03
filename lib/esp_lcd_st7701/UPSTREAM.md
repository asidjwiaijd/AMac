# ST7701 MIPI driver

Copied from Espressif esp-iot-solution commit
`8f2257f7e88aeba4c325fe288a5121b92900391c` (component 2.0.3).

Source: https://github.com/espressif/esp-iot-solution/tree/8f2257f7e88aeba4c325fe288a5121b92900391c/components/display/lcd/esp_lcd_st7701

Only the MIPI backend and its headers are included. aMac calls
`esp_lcd_new_panel_st7701_mipi` directly; the RGB backend and its SPI
dependencies are unnecessary. The driver's generic default table remains
unchanged; aMac overrides it with the supplied kernel DTSI's panel commands
and uses that file's timing. See hardware/reference/HD20034_KERNEL_REFERENCE.md.
The original Apache 2.0 license is in LICENSE.

The 480x360 macro is named 60HZ upstream, but its 15 MHz / 520 / 390 timing
evaluates to about 73.96 Hz. Those values belonged to aMac 0.1.0 only. aMac
0.1.1 uses the supplied panel's 15 MHz / 581 / 416 timing (about 62.06 Hz).
