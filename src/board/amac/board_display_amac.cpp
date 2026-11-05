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
#include "hd20034_panel_profile.h"

namespace Panel = AmacHd20034;
static_assert(Panel::kWidth == BOARD_DISPLAY_WIDTH && Panel::kHeight == BOARD_DISPLAY_HEIGHT,
              "Panel timing and framebuffer geometry must agree");
static st7701_lcd_init_cmd_t s_init_commands[Panel::kInitCommandCount];

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
    digitalWrite(AMAC_LCD_RESET_GPIO, LOW);
    pinMode(AMAC_LCD_RESET_GPIO, OUTPUT);

    esp_ldo_channel_config_t ldo = {};
    ldo.chan_id = 3;
    ldo.voltage_mv = 2500; // P4 MIPI D-PHY supply, NOT the panel VCI rail.
    esp_err_t err = esp_ldo_acquire_channel(&ldo, &s_phy_power);
    if (err != ESP_OK) goto failed;
    {
        // The upstream configuration macros use C-only nested designators.
        esp_lcd_dsi_bus_config_t bus = {};
        bus.bus_id = 0;
        bus.num_data_lanes = Panel::kDataLanes;
        bus.lane_bit_rate_mbps = Panel::kLaneRateMbps;
        err = esp_lcd_new_dsi_bus(&bus, &s_bus);
        if (err != ESP_OK) goto failed;
        esp_lcd_dbi_io_config_t dbi = ST7701_PANEL_IO_DBI_CONFIG();
        err = esp_lcd_new_panel_io_dbi(s_bus, &dbi, &s_io);
        if (err != ESP_OK) goto failed;

        // Supplied RK3566 display reference: 15 MHz / (581 * 416) ~= 62.06 Hz.
        esp_lcd_dpi_panel_config_t dpi = {};
        dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
        dpi.dpi_clock_freq_mhz = Panel::kPixelClockHz / 1000000.0f;
        dpi.virtual_channel = 0;
        dpi.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
        // The Linux reference uses RGB888. Keep this project's RGB565 buffer
        // and DSI packet format paired with COLMOD=0x55 (bits_per_pixel=16).
        dpi.num_fbs = 1;
        dpi.video_timing.h_size = BOARD_DISPLAY_WIDTH;
        dpi.video_timing.v_size = BOARD_DISPLAY_HEIGHT;
        dpi.video_timing.hsync_back_porch = Panel::kHBackPorch;
        dpi.video_timing.hsync_pulse_width = Panel::kHSync;
        dpi.video_timing.hsync_front_porch = Panel::kHFrontPorch;
        dpi.video_timing.vsync_back_porch = Panel::kVBackPorch;
        dpi.video_timing.vsync_pulse_width = Panel::kVSync;
        dpi.video_timing.vsync_front_porch = Panel::kVFrontPorch;
        dpi.flags.use_dma2d = false; // CPU writes directly into the one scanout surface.
        st7701_vendor_config_t vendor = {};
        for (size_t i = 0; i < Panel::kInitCommandCount; ++i) {
            const auto &cmd = Panel::kInitCommands[i];
            s_init_commands[i] = {cmd.command, cmd.data, cmd.data_bytes, cmd.delay_ms};
        }
        vendor.init_cmds = s_init_commands;
        vendor.init_cmds_size = Panel::kInitCommandCount;
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
        // Match simple-panel's active-low reset sequence and delays. The
        // generic ST7701 driver's fixed 10 ms reset is too short for this DTSI.
        delay(Panel::kPrepareDelayMs);
        digitalWrite(AMAC_LCD_RESET_GPIO, LOW);
        delay(Panel::kResetDelayMs);
        digitalWrite(AMAC_LCD_RESET_GPIO, HIGH);
        delay(Panel::kInitDelayMs);
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
        delay(Panel::kEnableDelayMs);
        BoardDisplay_SetBacklight(100);
        ESP_LOGI(TAG, "HD20034 %ux%u RGB565, %u Hz, %u lanes at %u Mbps, kernel panel init",
                 (unsigned)Panel::kWidth, (unsigned)Panel::kHeight, (unsigned)Panel::kPixelClockHz,
                 (unsigned)Panel::kDataLanes, (unsigned)Panel::kLaneRateMbps);
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

// Called after InputExit / VideoExit, with no remaining framebuffer users.
extern "C" void AmacDisplayPrepareDeepSleep()
{
    BoardDisplay_SetBacklight(0);
    if (s_io) {
        esp_lcd_panel_io_tx_param(s_io, 0x28, nullptr, 0); // display off
        esp_lcd_panel_io_tx_param(s_io, 0x10, nullptr, 0); // sleep in
        delay(120);
    }
    digitalWrite(AMAC_LCD_RESET_GPIO, LOW);
    releaseDisplay();
    s_initialized = false;
}

extern "C" bool AmacDisplayRecoverFromSleepFailure()
{
    releaseDisplay();
    s_initialized = false;
    return BoardDisplay_Init();
}
