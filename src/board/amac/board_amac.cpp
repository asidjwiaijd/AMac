#include "board.h"
#include "board_display.h"
#include "board_touch.h"
#include "board_keyboard.h"
#include "board_wifi.h"
#include <Arduino.h>

extern "C" void AmacWifiPrepare(void);

extern "C" bool Board_Init(void)
{
    static bool initialized = false;
    if (initialized) return true;
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
