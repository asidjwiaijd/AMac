#include "board.h"
#include "board_display.h"
#include "board_touch.h"
#include "board_keyboard.h"
#include "board_wifi.h"
#include <Arduino.h>
#include "esp_ldo_regulator.h"
#include "esp_log.h"

extern "C" void AmacWifiPrepare(void);

extern "C" bool Board_Init(void)
{
    static bool initialized = false;
    if (initialized) return true;
    // OSPTEK exposes LDO4 (module pad 66) for the SD IO voltage domain.
    // Keep it at 3.3V even though the card VDD and pull-ups are on fixed
    // VCC_3V3. Do not let the SD stack negotiate 1.8V on this PCB.
    static esp_ldo_channel_handle_t sd_io_power = nullptr;
    if (!sd_io_power) {
        esp_ldo_channel_config_t power = {};
        power.chan_id = 4;
        power.voltage_mv = 3300;
        const esp_err_t err = esp_ldo_acquire_channel(&power, &sd_io_power);
        if (err != ESP_OK) {
            ESP_LOGE("amac", "SD IO 3.3V supply: %s", esp_err_to_name(err));
            return false;
        }
    }
    // IR is a receiver output. Never drive this net (formerly PWM15_IR_M0).
    pinMode(AMAC_IR_GPIO, INPUT);
    if (!BoardDisplay_Init()) return false;
    BoardTouch_Init();
    initialized = true;
    return true;
}

extern "C" void Board_Update(void) {}

// Neither a touch controller nor a matrix keyboard exists in these netlists.
// USB HID and serial automation are handled by the shared input layer.
extern "C" bool BoardTouch_Init(void) { return true; }
extern "C" void BoardTouch_Update(void) {}
extern "C" BoardTouchDetail BoardTouch_GetDetail(void) { return {false, 0, 0, -1}; }
extern "C" void BoardTouch_GetMulti(BoardTouchMulti *out)
{
    if (!out) return;
    *out = {};
    for (auto &p : out->p) p.id = -1;
}
extern "C" bool BoardKeyboard_Init(void) { return true; }
extern "C" bool BoardKeyboard_Poll(BoardKeyboardEvent *) { return false; }
extern "C" bool BoardKeyboard_IsConnected(void) { return false; }
extern "C" void BoardKeyboard_Exit(void) {}

extern "C" void BoardWifi_Prepare(void)
{
    // Implemented in board_wifi_amac.cpp to keep WiFi headers out of this TU.
    AmacWifiPrepare();
}
extern "C" bool BoardWifi_IsCoprocessorFirmwareOutdated(void) { return false; }
extern "C" BoardWifiFirmwareResult BoardWifi_ApplyCoprocessorFirmware(void)
{
    // Do not stream upstream's C6 firmware into this board's C5.
    return BOARD_WIFI_FW_NOT_SUPPORTED;
}
