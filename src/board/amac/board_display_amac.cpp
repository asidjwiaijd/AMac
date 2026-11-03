#include "board_display.h"
#include "board_display_surface.h"
#include <Arduino.h>
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_st7701_interface.h"
#include "esp_log.h"

static const char *TAG = "amac_display";
static esp_lcd_dsi_bus_handle_t s_bus;
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static esp_ldo_channel_handle_t s_phy_power;
static bool s_initialized;
static bool s_flip;

static void releaseDisplay(void)
{
    if (s_panel) { esp_lcd_panel_del(s_panel); s_panel = nullptr; }
    if (s_io) { esp_lcd_panel_io_del(s_io); s_io = nullptr; }
    if (s_bus) { esp_lcd_del_dsi_bus(s_bus); s_bus = nullptr; }
    if (s_phy_power) { esp_ldo_release_channel(s_phy_power); s_phy_power = nullptr; }
}

extern "C" void BoardDisplay_SetBacklight(int percent)
{
    // The potentiometer sets LED current. This GPIO only gates the supply.
    digitalWrite(AMAC_LCD_BACKLIGHT_GPIO,
                 percent > 0 ? AMAC_BACKLIGHT_ON_LEVEL : !AMAC_BACKLIGHT_ON_LEVEL);
}

extern "C" bool BoardDisplay_Init(void)
{
    if (s_initialized) return true;
    digitalWrite(AMAC_LCD_BACKLIGHT_GPIO, !AMAC_BACKLIGHT_ON_LEVEL);
    pinMode(AMAC_LCD_BACKLIGHT_GPIO, OUTPUT);

    esp_ldo_channel_config_t ldo = {};
    ldo.chan_id = 3;
    ldo.voltage_mv = 2500; // P4 MIPI D-PHY supply, NOT the panel VCI rail.
    esp_err_t err = esp_ldo_acquire_channel(&ldo, &s_phy_power);
    if (err != ESP_OK) goto failed;
    {
        // The upstream configuration macros use C-only nested designators.
        esp_lcd_dsi_bus_config_t bus = {};
        bus.bus_id = 0;
        bus.num_data_lanes = 2;
        bus.lane_bit_rate_mbps = AMAC_LCD_LANE_RATE_MBPS;
        err = esp_lcd_new_dsi_bus(&bus, &s_bus);
        if (err != ESP_OK) goto failed;
        esp_lcd_dbi_io_config_t dbi = ST7701_PANEL_IO_DBI_CONFIG();
        err = esp_lcd_new_panel_io_dbi(s_bus, &dbi, &s_io);
        if (err != ESP_OK) goto failed;

        // Same timings as Espressif's ST7701_480_360 reference macro.
        // 15 MHz / (520 * 390) = about 73.96 Hz (the macro is named 60HZ).
        esp_lcd_dpi_panel_config_t dpi = {};
        dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
        dpi.dpi_clock_freq_mhz = AMAC_LCD_PIXEL_CLOCK_MHZ;
        dpi.virtual_channel = 0;
        dpi.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
        dpi.num_fbs = 1;
        dpi.video_timing.h_size = BOARD_DISPLAY_WIDTH;
        dpi.video_timing.v_size = BOARD_DISPLAY_HEIGHT;
        dpi.video_timing.hsync_back_porch = 10;
        dpi.video_timing.hsync_pulse_width = 10;
        dpi.video_timing.hsync_front_porch = 20;
        dpi.video_timing.vsync_back_porch = 10;
        dpi.video_timing.vsync_pulse_width = 10;
        dpi.video_timing.vsync_front_porch = 10;
        dpi.flags.use_dma2d = false; // CPU writes directly into the one scanout surface.
        st7701_vendor_config_t vendor = {};
        vendor.flags.use_mipi_interface = 1;
        vendor.mipi_config.dsi_bus = s_bus;
        vendor.mipi_config.dpi_config = &dpi;
        esp_lcd_panel_dev_config_t panel = {};
        panel.reset_gpio_num = AMAC_LCD_RESET_GPIO;
        panel.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel.bits_per_pixel = 16;
        panel.vendor_config = &vendor;
        err = esp_lcd_new_panel_st7701_mipi(s_io, &panel, &s_panel);
        if (err != ESP_OK) goto failed;
        err = esp_lcd_panel_reset(s_panel);
        if (err != ESP_OK) goto failed;
        err = esp_lcd_panel_init(s_panel);
        if (err != ESP_OK) goto failed;
        void *fb = nullptr;
        err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, &fb);
        if (err != ESP_OK) goto failed;
        if (!BoardDisplay_AttachSurface(fb, BOARD_DISPLAY_WIDTH, BOARD_DISPLAY_HEIGHT)) {
            err = ESP_ERR_NO_MEM;
            goto failed;
        }
        BoardDisplay_Gfx().setFlip180(s_flip);
        BoardDisplay_Present();
        // Init sequence already sends DISPON; retain the driver's panel handle.
        s_initialized = true;
        BoardDisplay_SetBacklight(100);
        ESP_LOGI(TAG, "HD20034 ST7701S 480x360 RGB565, 2 lanes, reference initialization");
        return true;
    }
failed:
    ESP_LOGE(TAG, "Display init: %s", esp_err_to_name(err));
    BoardDisplay_SetBacklight(0);
    releaseDisplay();
    return false;
}

extern "C" void BoardDisplay_SetFlip180(bool flip)
{
    s_flip = flip;
    if (s_initialized) BoardDisplay_Gfx().setFlip180(flip);
}
