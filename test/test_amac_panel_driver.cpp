#include <cassert>
#include <cstdio>
#include <vector>
#include "amac_panel_sdk.h"
#include "board_display.h"
#include "amac/hd20034_panel_profile.h"

struct Event { char kind; int pin; unsigned value; };
static std::vector<Event> events;
static std::vector<uint16_t> framebuffer(480 * 360);
static bool fail_panel = true;
static int deleted_io, deleted_bus, released_ldo, initialized;
static esp_lcd_dpi_panel_config_t dpi_config;
static std::vector<st7701_lcd_init_cmd_t> commands;

void digitalWrite(int pin, int level) { events.push_back({'G', pin, (unsigned)level}); }
void pinMode(int, int) {}
void delay(unsigned long ms) { events.push_back({'D', 0, (unsigned)ms}); }
esp_err_t esp_ldo_acquire_channel(const esp_ldo_channel_config_t *cfg, void **out)
{
    assert(cfg->chan_id == 3 && cfg->voltage_mv == 2500);
    *out = (void *)1;
    return ESP_OK;
}
esp_err_t esp_ldo_release_channel(void *) { ++released_ldo; return ESP_OK; }
esp_err_t esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t *cfg, void **out)
{
    assert(cfg->num_data_lanes == 2 && cfg->lane_bit_rate_mbps == 240);
    *out = (void *)2;
    return ESP_OK;
}
esp_err_t esp_lcd_del_dsi_bus(void *) { ++deleted_bus; return ESP_OK; }
esp_err_t esp_lcd_new_panel_io_dbi(void *, const esp_lcd_dbi_io_config_t *cfg, void **out)
{
    assert(cfg->virtual_channel == 0 && cfg->lcd_cmd_bits == 8 && cfg->lcd_param_bits == 8);
    *out = (void *)3;
    return ESP_OK;
}
esp_err_t esp_lcd_panel_io_del(void *) { ++deleted_io; return ESP_OK; }
esp_err_t esp_lcd_new_panel_st7701_mipi(void *, const esp_lcd_panel_dev_config_t *cfg, void **out)
{
    if (fail_panel) return ESP_ERR_NO_MEM;
    assert(cfg->reset_gpio_num == 23 && cfg->bits_per_pixel == 16);
    const auto &vendor = *(const st7701_vendor_config_t *)cfg->vendor_config;
    assert(vendor.flags.use_mipi_interface && vendor.init_cmds && vendor.init_cmds_size == 41);
    dpi_config = *vendor.mipi_config.dpi_config;
    commands.assign(vendor.init_cmds, vendor.init_cmds + vendor.init_cmds_size);
    *out = (void *)4;
    return ESP_OK;
}
esp_err_t esp_lcd_panel_init(void *) { ++initialized; events.push_back({'I', 0, 0}); return ESP_OK; }
esp_err_t esp_lcd_panel_del(void *) { return ESP_OK; }
esp_err_t esp_lcd_dpi_panel_get_frame_buffer(void *, uint32_t count, void **out)
{
    assert(count == 1);
    *out = framebuffer.data();
    return ESP_OK;
}

int main()
{
    // A failed creation must leave the backlight off and release resources.
    assert(!BoardDisplay_Init());
    assert(deleted_io == 1 && deleted_bus == 1 && released_ldo == 1 && initialized == 0);
    assert(events.back().kind == 'G' && events.back().pin == 13 && events.back().value == LOW);
    events.clear();
    fail_panel = false;
    assert(BoardDisplay_Init());
    assert(initialized == 1);
    assert(dpi_config.dpi_clock_freq_mhz == 15 && dpi_config.num_fbs == 1);
    assert(dpi_config.pixel_format == LCD_COLOR_PIXEL_FORMAT_RGB565);
    const auto &t = dpi_config.video_timing;
    assert(t.h_size == 480 && t.v_size == 360);
    assert(t.hsync_front_porch == 48 && t.hsync_back_porch == 43 && t.hsync_pulse_width == 10);
    assert(t.vsync_front_porch == 18 && t.vsync_back_porch == 28 && t.vsync_pulse_width == 10);
    for (size_t i = 0; i < commands.size(); ++i) {
        const auto &expected = AmacHd20034::kInitCommands[i];
        assert(commands[i].cmd == expected.command && commands[i].data_bytes == expected.data_bytes);
        assert(commands[i].delay_ms == expected.delay_ms);
        const auto *bytes = (const uint8_t *)commands[i].data;
        for (size_t j = 0; j < expected.data_bytes; ++j) assert(bytes[j] == expected.data[j]);
    }
    // Assert the active-low 200 ms pulse, 120 ms release delay, then init,
    // followed by a 150 ms wait before enabling the backlight.
    size_t i = 0;
    while (i < events.size() && events[i].kind != 'D') ++i;
    assert(i + 7 < events.size());
    assert(events[i++].value == 20);
    assert(events[i].kind == 'G' && events[i].pin == 23 && events[i++].value == LOW);
    assert(events[i++].value == 200);
    assert(events[i].kind == 'G' && events[i].pin == 23 && events[i++].value == HIGH);
    assert(events[i++].value == 120);
    assert(events[i++].kind == 'I');
    assert(events[i++].value == 150);
    assert(events[i].kind == 'G' && events[i].pin == 13 && events[i].value == HIGH);
    const size_t count = events.size();
    assert(BoardDisplay_Init() && events.size() == count); // bring-up is idempotent
    std::puts("panel bring-up: custom DCS table, timing, reset, backlight and cleanup OK");
}
