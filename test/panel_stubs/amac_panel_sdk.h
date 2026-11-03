#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_cache.h"

#define LOW 0
#define HIGH 1
#define OUTPUT 2
#define ESP_ERR_NO_MEM 0x101
void digitalWrite(int pin, int level);
void pinMode(int pin, int mode);
void delay(unsigned long ms);

using esp_lcd_dsi_bus_handle_t = void *;
using esp_lcd_panel_io_handle_t = void *;
using esp_lcd_panel_handle_t = void *;
using esp_ldo_channel_handle_t = void *;
enum { MIPI_DSI_DPI_CLK_SRC_DEFAULT, LCD_COLOR_PIXEL_FORMAT_RGB565, LCD_RGB_ELEMENT_ORDER_RGB };
struct esp_ldo_channel_config_t { int chan_id; int voltage_mv; };
struct esp_lcd_dsi_bus_config_t { int bus_id; uint8_t num_data_lanes; int phy_clk_src; float lane_bit_rate_mbps; };
struct esp_lcd_dbi_io_config_t { uint8_t virtual_channel; int lcd_cmd_bits; int lcd_param_bits; };
struct esp_lcd_video_timing_t {
    int h_size, v_size, hsync_back_porch, hsync_pulse_width, hsync_front_porch;
    int vsync_back_porch, vsync_pulse_width, vsync_front_porch;
};
struct esp_lcd_dpi_panel_config_t {
    uint8_t virtual_channel;
    int dpi_clk_src;
    float dpi_clock_freq_mhz;
    int pixel_format;
    uint8_t num_fbs;
    esp_lcd_video_timing_t video_timing;
    struct { bool use_dma2d; bool disable_lp; } flags;
};
struct st7701_lcd_init_cmd_t { int cmd; const void *data; size_t data_bytes; unsigned delay_ms; };
struct st7701_vendor_config_t {
    const st7701_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct { esp_lcd_dsi_bus_handle_t dsi_bus; const esp_lcd_dpi_panel_config_t *dpi_config; } mipi_config;
    struct { bool use_mipi_interface; } flags;
};
struct esp_lcd_panel_dev_config_t { int reset_gpio_num; int rgb_ele_order; int bits_per_pixel; void *vendor_config; };
#define ST7701_PANEL_IO_DBI_CONFIG() {0, 8, 8}

esp_err_t esp_ldo_acquire_channel(const esp_ldo_channel_config_t *, esp_ldo_channel_handle_t *);
esp_err_t esp_ldo_release_channel(esp_ldo_channel_handle_t);
esp_err_t esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t *, esp_lcd_dsi_bus_handle_t *);
esp_err_t esp_lcd_del_dsi_bus(esp_lcd_dsi_bus_handle_t);
esp_err_t esp_lcd_new_panel_io_dbi(esp_lcd_dsi_bus_handle_t, const esp_lcd_dbi_io_config_t *, esp_lcd_panel_io_handle_t *);
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t);
esp_err_t esp_lcd_new_panel_st7701_mipi(esp_lcd_panel_io_handle_t, const esp_lcd_panel_dev_config_t *, esp_lcd_panel_handle_t *);
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t);
esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t);
esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t, uint32_t, void **);
