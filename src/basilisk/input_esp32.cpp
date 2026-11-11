/*
 *  input_esp32.cpp - Input handling for ESP32 with M5Unified
 *
 *  BasiliskII ESP32 Port
 *
 *  Handles:
 *  - Touch panel input (as mouse via M5Unified)
 *  - USB HID keyboard input (via ESP-IDF USB Host Library)
 *  - USB HID mouse input (via ESP-IDF USB Host Library)
 *
 *  USB Host uses USB2 port on M5Stack Tab5
 *  Supports USB hubs for simultaneous keyboard + mouse
 *
 *  Mouse right-click is translated to Control+Click for Mac OS 8
 *  contextual menu compatibility.
 */

#include "sysdeps.h"
#include "input.h"
#include "adb.h"
#include "video.h"

#include "board.h"
#include "board_keyboard.h"
#include "board_touch.h"
#include "board_display.h"
#include "board_power.h"
#include "power_manager.h"
#include "low_battery.h"
#include "touch_overlay.h"
#include "birthday_gift_runtime.h"

#if defined(BOARD_M5STACK_TAB5)
#include <M5Unified.h>
#endif
#include <usb/usb_host.h>
#include <class/hid/hid.h>
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "hid_descriptor.h"
#include "boot_gui.h"
#if defined(BOARD_AMAC_P4)
#include "hal/usb_dwc_ll.h"
#endif

#ifndef USB_INTERFACE_DESC
#define USB_INTERFACE_DESC 0x04
#endif
#ifndef USB_ENDPOINT_DESC
#define USB_ENDPOINT_DESC 0x05
#endif
/* HID class descriptors per HID 1.11 §7.1: 0x21 = HID, 0x22 = Report. */
#ifndef USB_HID_DESC_TYPE
#define USB_HID_DESC_TYPE        0x21
#endif
#ifndef USB_HID_REPORT_DESC_TYPE
#define USB_HID_REPORT_DESC_TYPE 0x22
#endif

/* Maximum HID report descriptor we will accept. Real-world mice top out
 * around 100-200 bytes; allocate a comfortable upper bound. */
#define HID_REPORT_DESC_MAX_BYTES 512

#define DEBUG 0
#include "debug.h"

// ============================================================================
// Input Task Configuration (runs on Core 0 to offload CPU emulation)
// ============================================================================
#define INPUT_TASK_STACK_SIZE 8192
#define INPUT_TASK_PRIORITY   3
#define INPUT_TASK_CORE       0  // Run on Core 0, leaving Core 1 for CPU emulation
#define INPUT_POLL_INTERVAL_MS 1
#define BOARD_INPUT_INTERVAL_MS 20

static TaskHandle_t input_task_handle = NULL;
static SemaphoreHandle_t input_task_stopped = NULL;
static volatile bool input_task_running = false;
static volatile bool input_preboot = false;
static int preboot_mouse_x = BOARD_MAC_SCREEN_WIDTH / 2;
static int preboot_mouse_y = BOARD_MAC_SCREEN_HEIGHT / 2;
static volatile uint32_t usb_mouse_reports = 0;
static uint32_t usb_rejected_reports = 0, usb_transfer_errors = 0;
static uint32_t usb_max_service_gap_us = 0;
struct UsbMouseTrace {
    uint32_t sequence, ms;
    uint8_t endpoint, interface_number, size, accepted, buttons;
    int16_t dx, dy, wheel;
    uint8_t raw[16];
};
static UsbMouseTrace usb_trace[32] = {};
static uint32_t usb_trace_sequence = 0;
static portMUX_TYPE usb_trace_lock = portMUX_INITIALIZER_UNLOCKED;
static void noteUsbTransferError() { __atomic_add_fetch(&usb_transfer_errors,1,__ATOMIC_RELAXED); }
static void noteUsbServiceGap(uint32_t gap)
{
    uint32_t previous=__atomic_load_n(&usb_max_service_gap_us,__ATOMIC_RELAXED);
    while (gap>previous && !__atomic_compare_exchange_n(&usb_max_service_gap_us,&previous,gap,
            false,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
}

// ============================================================================
// USB HID Scancode to Mac ADB Keycode Translation Table
// ============================================================================
//
// USB HID scancodes (Usage Page 0x07) map to Mac ADB keycodes.
// This table is based on the SDL2/cocoa keycode mapping from BasiliskII.
// Index = USB HID scancode, Value = Mac ADB keycode (0xFF = invalid/unmapped)
//
// Reference: USB HID Usage Tables, Keyboard/Keypad Page (0x07)
// https://usb.org/sites/default/files/hut1_4.pdf
//

// USB HID to Mac ADB keycode translation - in internal SRAM for fast lookup
// Accessed on every keystroke
DRAM_ATTR static const uint8_t usb_to_mac_keycode[256] = {
    // 0x00-0x03: Reserved/Error codes
    0xFF, 0xFF, 0xFF, 0xFF,
    
    // 0x04-0x1D: Letters A-Z
    0x00,  // 0x04: A
    0x0B,  // 0x05: B
    0x08,  // 0x06: C
    0x02,  // 0x07: D
    0x0E,  // 0x08: E
    0x03,  // 0x09: F
    0x05,  // 0x0A: G
    0x04,  // 0x0B: H
    0x22,  // 0x0C: I
    0x26,  // 0x0D: J
    0x28,  // 0x0E: K
    0x25,  // 0x0F: L
    0x2E,  // 0x10: M
    0x2D,  // 0x11: N
    0x1F,  // 0x12: O
    0x23,  // 0x13: P
    0x0C,  // 0x14: Q
    0x0F,  // 0x15: R
    0x01,  // 0x16: S
    0x11,  // 0x17: T
    0x20,  // 0x18: U
    0x09,  // 0x19: V
    0x0D,  // 0x1A: W
    0x07,  // 0x1B: X
    0x10,  // 0x1C: Y
    0x06,  // 0x1D: Z
    
    // 0x1E-0x27: Numbers 1-9, 0
    0x12,  // 0x1E: 1
    0x13,  // 0x1F: 2
    0x14,  // 0x20: 3
    0x15,  // 0x21: 4
    0x17,  // 0x22: 5
    0x16,  // 0x23: 6
    0x1A,  // 0x24: 7
    0x1C,  // 0x25: 8
    0x19,  // 0x26: 9
    0x1D,  // 0x27: 0
    
    // 0x28-0x2C: Special keys
    0x24,  // 0x28: Return/Enter
    0x35,  // 0x29: Escape
    0x33,  // 0x2A: Backspace/Delete
    0x30,  // 0x2B: Tab
    0x31,  // 0x2C: Space
    
    // 0x2D-0x38: Punctuation and symbols
    0x1B,  // 0x2D: - (minus)
    0x18,  // 0x2E: = (equals)
    0x21,  // 0x2F: [ (left bracket)
    0x1E,  // 0x30: ] (right bracket)
    0x2A,  // 0x31: \ (backslash)
    0x32,  // 0x32: # (non-US hash) - maps to International
    0x29,  // 0x33: ; (semicolon)
    0x27,  // 0x34: ' (apostrophe)
    0x0A,  // 0x35: ` (grave accent)
    0x2B,  // 0x36: , (comma)
    0x2F,  // 0x37: . (period)
    0x2C,  // 0x38: / (slash)
    
    // 0x39: Caps Lock
    0x39,  // 0x39: Caps Lock
    
    // 0x3A-0x45: Function keys F1-F12
    0x7A,  // 0x3A: F1
    0x78,  // 0x3B: F2
    0x63,  // 0x3C: F3
    0x76,  // 0x3D: F4
    0x60,  // 0x3E: F5
    0x61,  // 0x3F: F6
    0x62,  // 0x40: F7
    0x64,  // 0x41: F8
    0x65,  // 0x42: F9
    0x6D,  // 0x43: F10
    0x67,  // 0x44: F11
    0x6F,  // 0x45: F12
    
    // 0x46-0x48: Print Screen, Scroll Lock, Pause
    0x69,  // 0x46: Print Screen (F13)
    0x6B,  // 0x47: Scroll Lock (F14)
    0x71,  // 0x48: Pause (F15)
    
    // 0x49-0x4E: Navigation cluster
    0x72,  // 0x49: Insert (Help)
    0x73,  // 0x4A: Home
    0x74,  // 0x4B: Page Up
    0x75,  // 0x4C: Delete (Forward Delete)
    0x77,  // 0x4D: End
    0x79,  // 0x4E: Page Down
    
    // 0x4F-0x52: Arrow keys
    0x3C,  // 0x4F: Right Arrow
    0x3B,  // 0x50: Left Arrow
    0x3D,  // 0x51: Down Arrow
    0x3E,  // 0x52: Up Arrow
    
    // 0x53: Num Lock
    0x47,  // 0x53: Num Lock/Clear
    
    // 0x54-0x63: Keypad
    0x4B,  // 0x54: KP /
    0x43,  // 0x55: KP *
    0x4E,  // 0x56: KP -
    0x45,  // 0x57: KP +
    0x4C,  // 0x58: KP Enter
    0x53,  // 0x59: KP 1
    0x54,  // 0x5A: KP 2
    0x55,  // 0x5B: KP 3
    0x56,  // 0x5C: KP 4
    0x57,  // 0x5D: KP 5
    0x58,  // 0x5E: KP 6
    0x59,  // 0x5F: KP 7
    0x5B,  // 0x60: KP 8
    0x5C,  // 0x61: KP 9
    0x52,  // 0x62: KP 0
    0x41,  // 0x63: KP .
    
    // 0x64: Non-US backslash
    0x32,  // 0x64: International
    
    // 0x65: Application/Menu key
    0x32,  // 0x65: Application (-> International)
    
    // 0x66: Power key
    0x7F,  // 0x66: Power
    
    // 0x67: KP =
    0x51,  // 0x67: KP =
    
    // 0x68-0x73: F13-F24 (extended function keys)
    0x69,  // 0x68: F13
    0x6B,  // 0x69: F14
    0x71,  // 0x6A: F15
    0xFF,  // 0x6B: F16
    0xFF,  // 0x6C: F17
    0xFF,  // 0x6D: F18
    0xFF,  // 0x6E: F19
    0xFF,  // 0x6F: F20
    0xFF,  // 0x70: F21
    0xFF,  // 0x71: F22
    0xFF,  // 0x72: F23
    0xFF,  // 0x73: F24
    
    // 0x74-0xDF: Various (mostly unmapped)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x74-0x7B
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x7C-0x83
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x84-0x8B
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x8C-0x93
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x94-0x9B
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x9C-0xA3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xA4-0xAB
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xAC-0xB3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xB4-0xBB
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xBC-0xC3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xC4-0xCB
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xCC-0xD3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xD4-0xDB
    0xFF, 0xFF, 0xFF, 0xFF,                          // 0xDC-0xDF
    
    // 0xE0-0xE7: Modifier keys (left/right variants)
    0x36,  // 0xE0: Left Control
    0x38,  // 0xE1: Left Shift
    0x3A,  // 0xE2: Left Alt (-> Option)
    0x37,  // 0xE3: Left GUI/Command
    0x36,  // 0xE4: Right Control
    0x38,  // 0xE5: Right Shift
    0x3A,  // 0xE6: Right Alt (-> Option)
    0x37,  // 0xE7: Right GUI/Command
    
    // 0xE8-0xFF: Reserved
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xE8-0xEF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xF0-0xF7
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   // 0xF8-0xFF
};

// ============================================================================
// Input State
// ============================================================================

// Mac screen dimensions for coordinate scaling
/* Mac framebuffer dimensions come from the board config (640x360 on Tab5,
 * 640x400 on Waveshare). InputSetScreenSize() can still override at runtime
 * if the emulator switches modes. */
static int mac_screen_width  = BOARD_MAC_SCREEN_WIDTH;
static int mac_screen_height = BOARD_MAC_SCREEN_HEIGHT;

// Display dimensions (populated from BoardDisplay_* in InputInit)
static int display_width  = BOARD_DISPLAY_WIDTH;
static int display_height = BOARD_DISPLAY_HEIGHT;

// Input enable flags
static bool keyboard_enabled = true;
// Touch enable is owned by touch_overlay.cpp now; we keep a local shadow
// for InputSetTouchEnabled() to forward without pulling in a getter.
static bool touch_enabled = true;

// USB device connection state
static bool usb_keyboard_connected = false;
static bool mouse_connected = false;

// USB mouse button state
static uint8_t usb_mouse_buttons = 0;

// Right-click to Control+Click translation state (Mac OS 8 contextual menus)
static bool right_click_ctrl_injected = false;

#include "input_shared_state.inc"

// LED state tracking
static uint8_t last_led_state = 0;
static uint32_t last_led_check_time = 0;
static const uint32_t LED_CHECK_INTERVAL_MS = 100;  // Check every 100ms

// ============================================================================
// Multi-Device USB Host Types
// ============================================================================

#include "input_usb_types.h"

// Forward declarations
class MultiDeviceUsbHost;
static MultiDeviceUsbHost *usbHost = NULL;
static uint8_t mergedUsbMouseButtons();
static void releaseDisconnectedMouseButtons();

// ============================================================================
// Keyboard Input Processing
// ============================================================================

static bool isControlPhysicallyHeld() {
    return controlKeyClaimed();
}

static void processKeyboardReport(hid_keyboard_report_t *report,
                                   hid_keyboard_report_t *last_report,
                                   bool force_release = false) {
    if (input_preboot) return; // ADB and the guest do not exist yet.
    if (!keyboard_enabled && !force_release) return;

    static const uint8_t modifier_to_mac[8] = {
        0x36, 0x38, 0x3A, 0x37,  // left Ctrl, Shift, Alt/Option, GUI/Command
        0x36, 0x38, 0x3A, 0x37   // right variants
    };

    // Every USB slot owns its own last report, so modifiers held by two
    // keyboards remain independent claims even when they map to one ADB key.
    for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint8_t mask = (uint8_t)(1U << bit);
        const bool was_pressed = (last_report->modifier & mask) != 0;
        const bool is_pressed  = (report->modifier & mask) != 0;
        if (is_pressed && !was_pressed) {
            claimAdbKey(modifier_to_mac[bit]);
        } else if (!is_pressed && was_pressed) {
            releaseAdbKey(modifier_to_mac[bit]);
        }
    }

    // Process key releases BEFORE key presses (important for key transitions)
    for (int i = 0; i < 6; i++) {
        uint8_t old_key = last_report->keycode[i];
        if (old_key == 0) continue;

        bool still_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (report->keycode[j] == old_key) {
                still_pressed = true;
                break;
            }
        }

        if (!still_pressed) {
            uint8_t mac_code = usb_to_mac_keycode[old_key];
            if (mac_code != 0xFF) {
                releaseAdbKey(mac_code);
            }
        }
    }

    // Process key presses
    for (int i = 0; i < 6; i++) {
        uint8_t new_key = report->keycode[i];
        if (new_key == 0) continue;

        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (last_report->keycode[j] == new_key) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed) {
            uint8_t mac_code = usb_to_mac_keycode[new_key];
            if (mac_code != 0xFF) {
                claimAdbKey(mac_code);
            }
        }
    }
}

// ============================================================================
// Official M5Stack Tab5 Keyboard (Ext.Port1 I2C matrix)
// ============================================================================

struct Tab5HidMapping {
    uint8_t usage;
    uint8_t modifiers;
};

/* Official US-layout matrix mapping published in M5Unit-KEYBOARD. Normal
 * mode is used instead of the module's lossy one-key HID event mode so every
 * matrix key and modifier has an independent press/release claim. */
static const Tab5HidMapping tab5_base_map[70] = {
    // Row 0: Esc 1 2 3 4 5 6 7 8 9 0 - + Del
    {0x29,0}, {0x1E,0}, {0x1F,0}, {0x20,0}, {0x21,0}, {0x22,0}, {0x23,0},
    {0x24,0}, {0x25,0}, {0x26,0}, {0x27,0}, {0x2D,0}, {0x2E,0x02}, {0x4C,0},
    // Row 1: ` ! @ # $ % ^ & * ( ) [ ] backslash
    {0x35,0}, {0x1E,0x02}, {0x1F,0x02}, {0x20,0x02}, {0x21,0x02},
    {0x22,0x02}, {0x23,0x02}, {0x24,0x02}, {0x25,0x02}, {0x26,0x02},
    {0x27,0x02}, {0x2F,0}, {0x30,0}, {0x31,0},
    // Row 2: Tab q w e r t y u i o p ; ' Backspace
    {0x2B,0}, {0x14,0}, {0x1A,0}, {0x08,0}, {0x15,0}, {0x17,0}, {0x1C,0},
    {0x18,0}, {0x0C,0}, {0x12,0}, {0x13,0}, {0x33,0}, {0x34,0}, {0x2A,0},
    // Row 3: Sym Aa a s d f g h j k l Up _ Enter
    {0,0}, {0,0}, {0x04,0}, {0x16,0}, {0x07,0}, {0x09,0}, {0x0A,0},
    {0x0B,0}, {0x0D,0}, {0x0E,0}, {0x0F,0}, {0x52,0}, {0x2D,0x02}, {0x28,0},
    // Row 4: Ctrl Alt z x c v b n m . Left Down Right Space
    {0,0}, {0,0}, {0x1D,0}, {0x1B,0}, {0x06,0}, {0x19,0}, {0x05,0},
    {0x11,0}, {0x10,0}, {0x37,0}, {0x50,0}, {0x51,0}, {0x4F,0}, {0x2C,0}
};

static_assert(sizeof(tab5_base_map) / sizeof(tab5_base_map[0]) == 70,
              "Tab5 keyboard matrix must contain 70 keys");

static Tab5HidMapping tab5Mapping(uint8_t row, uint8_t col, bool sym)
{
    const uint8_t index = (uint8_t)(row * 14U + col);
    Tab5HidMapping mapping = tab5_base_map[index];
    if (!sym) return mapping;

    // Sym-layer differences from the base printing on the physical keys.
    switch (index) {
        case 14: mapping = {0x35,0x02}; break; // ` -> ~
        case 15: mapping = {0x38,0x02}; break; // ! -> ?
        case 22: mapping = {0x38,0x00}; break; // * -> /
        case 23: mapping = {0x36,0x02}; break; // ( -> <
        case 24: mapping = {0x37,0x02}; break; // ) -> >
        case 25: mapping = {0x2F,0x02}; break; // [ -> {
        case 26: mapping = {0x30,0x02}; break; // ] -> }
        case 27: mapping = {0x31,0x02}; break; // backslash -> |
        case 39: mapping = {0x33,0x02}; break; // ; -> :
        case 40: mapping = {0x34,0x02}; break; // ' -> "
        case 54: mapping = {0x2E,0x00}; break; // _ -> =
        case 65: mapping = {0x36,0x00}; break; // . -> ,
        default: break;
    }
    return mapping;
}

struct Tab5KeyBinding {
    bool active;
    uint8_t mac_keycode;
    uint8_t forced_modifiers;
};

static Tab5KeyBinding tab5_bindings[70] = {};
static bool tab5_sym_active = false;
static bool tab5_keyboard_connected = false;

static const uint8_t hid_modifier_to_mac[8] = {
    0x36, 0x38, 0x3A, 0x37, 0x36, 0x38, 0x3A, 0x37
};

static void applyForcedModifiers(uint8_t modifiers, bool pressed)
{
    for (uint8_t bit = 0; bit < 8; ++bit) {
        if ((modifiers & (1U << bit)) == 0) continue;
        if (pressed) claimAdbKey(hid_modifier_to_mac[bit]);
        else releaseAdbKey(hid_modifier_to_mac[bit]);
    }
}

static bool isTab5ModifierIndex(uint8_t index)
{
    return index == 42 || index == 43 || index == 56 || index == 57;
}

static void releaseTab5Binding(uint8_t index)
{
    Tab5KeyBinding &binding = tab5_bindings[index];
    if (!binding.active) return;
    if (binding.mac_keycode != 0xFF) {
        releaseAdbKey(binding.mac_keycode);
    }
    applyForcedModifiers(binding.forced_modifiers, false);
    binding = {};
}

static void releaseAllTab5Bindings(void)
{
    // Release ordinary keys before their physical/synthetic modifiers.
    for (uint8_t i = 0; i < 70; ++i) {
        if (!isTab5ModifierIndex(i)) releaseTab5Binding(i);
    }
    for (uint8_t i = 0; i < 70; ++i) {
        if (isTab5ModifierIndex(i)) releaseTab5Binding(i);
    }
    tab5_sym_active = false;
}

static void processTab5KeyboardEvent(const BoardKeyboardEvent &event)
{
    if (event.row >= 5 || event.col >= 14) return;
    const uint8_t index = (uint8_t)(event.row * 14U + event.col);
    Tab5KeyBinding &binding = tab5_bindings[index];

    if (event.pressed == binding.active) {
        return; // Duplicate down or orphan up from a stale/replayed FIFO event.
    }
    if (!event.pressed) {
        releaseTab5Binding(index);
        if (index == 42) tab5_sym_active = false;
        return;
    }

    binding.active = true;
    binding.mac_keycode = 0xFF;
    binding.forced_modifiers = 0;

    switch (index) {
        case 42: // Sym selects the alternate printed layer; it is not a Mac key.
            tab5_sym_active = true;
            return;
        case 43: binding.mac_keycode = 0x38; break; // Aa -> Shift
        case 56: binding.mac_keycode = 0x36; break; // Ctrl
        case 57: binding.mac_keycode = 0x3A; break; // Alt -> Option
        default: {
            const Tab5HidMapping mapping = tab5Mapping(event.row, event.col,
                                                       tab5_sym_active);
            const uint8_t mac = usb_to_mac_keycode[mapping.usage];
            if (mapping.usage == 0 || mac == 0xFF) {
                binding = {};
                return;
            }
            binding.mac_keycode = mac;
            binding.forced_modifiers = mapping.modifiers;
            applyForcedModifiers(binding.forced_modifiers, true);
            break;
        }
    }
    claimAdbKey(binding.mac_keycode);
}

static void pollTab5Keyboard(void)
{
    BoardKeyboardEvent event = {};
    int event_budget = 32;
    while (event_budget-- > 0 && BoardKeyboard_Poll(&event)) {
        processTab5KeyboardEvent(event);
    }

    const bool connected_now = BoardKeyboard_IsConnected();
    if (tab5_keyboard_connected && !connected_now) {
        releaseAllTab5Bindings();
    }
    tab5_keyboard_connected = connected_now;
}

// ============================================================================
// Mouse Input Processing (with Mac OS 8 right-click -> Control+Click)
// ============================================================================

static void processMouseReport(const usb_transfer_t *transfer, EndpointInfo *ep_info) {
    if (ep_info->bInterfaceClass != USB_CLASS_HID) return;
    if (ep_info->bInterfaceProtocol == HID_ITF_PROTOCOL_KEYBOARD) return;

    uint8_t buttons = 0;
    int16_t dx = 0;
    int16_t dy = 0;
    int16_t dw = 0;

    // Decode using this endpoint's interface descriptor. No byte-layout
    // guessing while enumeration is pending or for vendor-only interfaces.
    bool decoded = ep_info->mouse.decode(transfer->data_buffer,
        transfer->actual_num_bytes,&buttons,&dx,&dy,&dw);
    UsbMouseTrace sample = {};
    sample.ms=millis(); sample.endpoint=transfer->bEndpointAddress;
    sample.interface_number=ep_info->interface_number;
    sample.size=(uint8_t)transfer->actual_num_bytes; sample.accepted=decoded;
    sample.buttons=buttons; sample.dx=dx; sample.dy=dy; sample.wheel=dw;
    memcpy(sample.raw,transfer->data_buffer,
           transfer->actual_num_bytes < sizeof(sample.raw) ? transfer->actual_num_bytes : sizeof(sample.raw));
    portENTER_CRITICAL(&usb_trace_lock);
    sample.sequence=++usb_trace_sequence;
    usb_trace[(sample.sequence-1)%32]=sample;
    portEXIT_CRITICAL(&usb_trace_lock);
    if (!decoded) { __atomic_add_fetch(&usb_rejected_reports,1,__ATOMIC_RELAXED); return; }
    ep_info->mouse.buttons=buttons;
    buttons=mergedUsbMouseButtons();
    mouse_connected = true;

    __atomic_add_fetch(&usb_mouse_reports,1,__ATOMIC_RELAXED);
    if (input_preboot) {
        preboot_mouse_x = constrain(preboot_mouse_x + dx, 0, mac_screen_width - 1);
        preboot_mouse_y = constrain(preboot_mouse_y + dy, 0, mac_screen_height - 1);
        BootGUI_UsbPointer(preboot_mouse_x, preboot_mouse_y, (buttons & 1) != 0);
        return;
    }

    // A HID packet can move and click at once. Update the gift's pointer
    // before hit testing that packet's button transitions.
    if (dx != 0 || dy != 0) routeMouseMove(dx, dy, true);

    uint8_t old_buttons = usb_mouse_buttons;

    // Mac OS 8 right-click -> Control+Click translation.
    // Inject/release a synthetic Control key when right button changes.
    bool right_now = (buttons & 0x02) != 0;
    bool right_was = (old_buttons & 0x02) != 0;

    if (right_now && !right_was) {
        if (!isControlPhysicallyHeld()) {
            claimAdbKey(0x36);  // synthetic Control claim
            right_click_ctrl_injected = true;
        }
    } else if (!right_now && right_was) {
        if (right_click_ctrl_injected) {
            releaseAdbKey(0x36);
            right_click_ctrl_injected = false;
        }
    }

    // Mac button 0 maps from USB left (0x01) OR right (0x02).
    // Both physical buttons drive a single Mac left-click so that
    // releasing one while the other is still held keeps the button down.
    bool mac_btn0_now = (buttons & 0x01) || (buttons & 0x02);
    bool mac_btn0_was = (old_buttons & 0x01) || (old_buttons & 0x02);

    if (mac_btn0_now && !mac_btn0_was) {
        routeMouseButton(0, true);
    } else if (!mac_btn0_now && mac_btn0_was) {
        routeMouseButton(0, false);
    }

    // Middle button (0x04) passes through as Mac button 2
    bool mid_now = (buttons & 0x04) != 0;
    bool mid_was = (old_buttons & 0x04) != 0;

    if (mid_now && !mid_was) {
        routeMouseButton(2, true);
    } else if (!mid_now && mid_was) {
        routeMouseButton(2, false);
    }

    usb_mouse_buttons = buttons;

    /* Forward scroll wheel as Mac arrow keys. Classic Mac OS doesn't
     * emulate ADB scroll natively (it pre-dates the wheel), so we
     * drive arrow-up/arrow-down keystrokes per wheel detent. Most
     * scroll-aware apps from the era (Netscape, IE 5, scrollable
     * dialogs) handle that fine; games can ignore. We rate-limit to
     * one keystroke per wheel detent rather than streaming so a
     * single notch produces one arrow press. */
    if (dw != 0) {
        const uint8_t mac_arrow_up   = 0x7E;
        const uint8_t mac_arrow_down = 0x7D;
        int16_t notches = dw;
        if (notches > 4)  notches = 4;
        if (notches < -4) notches = -4;
        while (notches > 0) {
            claimAdbKey(mac_arrow_up);
            releaseAdbKey(mac_arrow_up);
            notches--;
        }
        while (notches < 0) {
            claimAdbKey(mac_arrow_down);
            releaseAdbKey(mac_arrow_down);
            notches++;
        }
    }
}

// ============================================================================
// MultiDeviceUsbHost - USB Host with hub and multi-device support
// ============================================================================

#include "input_usb_host.inc"

static uint8_t mergedUsbMouseButtons()
{
    uint8_t buttons=0;
    if (usbHost) for (auto &dev:usbHost->devices) if (dev.active && !dev.disconnecting)
        for (auto &ep:dev.endpoint_info) buttons |= ep.mouse.buttons;
    return buttons;
}

static void releaseDisconnectedMouseButtons()
{
    uint8_t buttons=mergedUsbMouseButtons();
    if (input_preboot) {
        BootGUI_UsbPointer(mouse_connected ? preboot_mouse_x : -1,
                           mouse_connected ? preboot_mouse_y : -1,(buttons&1)!=0);
    } else {
        if ((usb_mouse_buttons&3) && !(buttons&3)) routeMouseButton(0,false);
        if ((usb_mouse_buttons&4) && !(buttons&4)) routeMouseButton(2,false);
        if (!(buttons&2) && right_click_ctrl_injected) {
            releaseAdbKey(0x36);right_click_ctrl_injected=false;
        }
    }
    usb_mouse_buttons=buttons;
}

// ============================================================================
// Touch Input Handling
// ============================================================================
//
// All touch handling (single-finger mouse, 3/4-finger gesture, overlay key
// press/release, 25% stipple compositor hooks) now lives in touch_overlay.cpp
// so the mouse pipeline and the overlay pipeline share one view of the
// touchscreen. We pump it here with the current multi-point state.

static void processTouchInput(void)
{
    if (!touch_enabled) return;

    BoardTouchMulti multi;
    BoardTouch_GetMulti(&multi);

    // A touch while the display is off wakes it. The Mac never sees that
    // touch: it is held back until every finger has lifted.
    static bool swallow_touch = false;
    if (multi.count > 0) {
        if (!swallow_touch && PowerManagerSleeping()) swallow_touch = true;
        PowerManagerNoteActivity();
    } else if (swallow_touch) {
        swallow_touch = false;
        return;
    }
    if (swallow_touch) return;
    TouchOverlay_Update(&multi);
}

/*
 *  Check and update keyboard LED state
 */
static void updateKeyboardLEDs(void)
{
    if (usbHost == NULL || !usbHost->hasAnyKeyboard()) {
        return;
    }
    
    uint32_t now = millis();
    if ((now - last_led_check_time) < LED_CHECK_INTERVAL_MS) {
        return;
    }
    last_led_check_time = now;
    
    uint8_t current_leds = ADBGetKeyboardLEDs();
    
    if (current_leds != last_led_state) {
        usbHost->setKeyboardLEDs(current_leds);
        last_led_state = current_leds;
    }
}

// ============================================================================
// Input Task (runs on Core 0)
// ============================================================================

/*
 *  Input polling task - runs on Core 0 independently of CPU emulation
 *  This offloads the ~2.3ms USB host processing from the CPU emulation loop
 */
static void inputTask(void *param)
{
    (void)param;
    Serial.println("[INPUT] Input task started on Core 0");
    
    const TickType_t poll_interval = pdMS_TO_TICKS(INPUT_POLL_INTERVAL_MS) < 1 ?
        1 : pdMS_TO_TICKS(INPUT_POLL_INTERVAL_MS);
    uint32_t next_board_poll = millis();
    
    while (input_task_running) {
        uint32_t now = millis();
        if ((int32_t)(now-next_board_poll)>=0) {
            next_board_poll=now+BOARD_INPUT_INTERVAL_MS;
            Board_Update();
            if (!input_preboot) {
                BoardPower_Poll();
#if defined(BOARD_AMAC_P4)
                LowBatteryPoll();
#endif
                PowerManagerIdlePoll();
                processTouchInput();
                pollTab5Keyboard();
            }
        }
        
        if (usbHost != NULL) {
            usbHost->task();
        }
        
        if (!input_preboot) updateKeyboardLEDs();
        
        vTaskDelay(poll_interval);
    }
    
    input_task_handle = NULL;
    if (input_task_stopped != NULL) {
        xSemaphoreGive(input_task_stopped);
    }
    Serial.println("[INPUT] Input task exiting");
    vTaskDelete(NULL);
}

// ============================================================================
// Public API
// ============================================================================

bool InputInit(void)
{
    // Stop callbacks while resetting guest-facing state; preserve the host
    // and the already enumerated hub/devices.
    if (input_task_running) {
        input_task_running = false;
        if (xSemaphoreTake(input_task_stopped, pdMS_TO_TICKS(5000)) != pdTRUE) return false;
    }
    input_preboot = false;
    BootGUI_UsbPointer(-1, -1, false);
    Serial.println("[INPUT] Initializing input subsystem...");
    
    display_width  = BoardDisplay_Width();
    display_height = BoardDisplay_Height();
    
    Serial.printf("[INPUT] Display size: %dx%d\n", display_width, display_height);
    Serial.printf("[INPUT] Mac screen size: %dx%d\n", mac_screen_width, mac_screen_height);

    last_led_state = 0;
    last_led_check_time = 0;
    resetInputClaims();
    BirthdayGift_Init();
    memset(tab5_bindings, 0, sizeof(tab5_bindings));
    tab5_sym_active = false;
    tab5_keyboard_connected = false;
    usb_keyboard_connected = false;

    ADBSetRelMouseMode(false);

    /* Overlay module owns mouse routing, multi-touch gesture detection,
     * and on-screen keyboard key dispatch. Initialize it with the actual
     * physical display size and Mac framebuffer size so the stipple
     * compositor and mouse-coordinate scaling are both correct. */
    TouchOverlay_Init(display_width, display_height,
                      mac_screen_width, mac_screen_height);

    Serial.println("[INPUT] Touch input enabled (multi-touch overlay ready)");

    if (!BoardKeyboard_Init()) {
        Serial.println("[INPUT] WARN: Tab5 Keyboard bus unavailable; continuing without it");
    }
    tab5_keyboard_connected = BoardKeyboard_IsConnected();
    
    Serial.println("[INPUT] Initializing USB Host (hub support enabled)...");
    if (!usbHost) {
        usbHost = new MultiDeviceUsbHost();
        if (usbHost) usbHost->begin();
    }
    if (usbHost != NULL && usbHost->clientHandle) {
        Serial.println("[INPUT] USB Host initialized - connect keyboard/mouse (hub supported)");
    } else {
        Serial.println("[INPUT] ERROR: Failed to create USB Host instance");
    }
    
    if (input_task_stopped == NULL) {
        input_task_stopped = xSemaphoreCreateBinary();
    }
    if (input_task_stopped == NULL) {
        Serial.println("[INPUT] ERROR: Failed to create task-stop semaphore");
        return false;
    }

    input_task_running = true;
    BaseType_t result = xTaskCreatePinnedToCore(
        inputTask,
        "InputTask",
        INPUT_TASK_STACK_SIZE,
        NULL,
        INPUT_TASK_PRIORITY,
        &input_task_handle,
        INPUT_TASK_CORE
    );
    
    if (result != pdPASS) {
        Serial.println("[INPUT] ERROR: Failed to create input task");
        input_task_running = false;
    } else {
        Serial.printf("[INPUT] Input task created on Core %d\n", INPUT_TASK_CORE);
    }
    
    return true;
}

bool InputInitPreboot(void)
{
    if (input_task_running) return true;
    input_preboot = true;
    if (!usbHost) {
        usbHost = new MultiDeviceUsbHost();
        if (usbHost) usbHost->begin();
    }
    if (!usbHost || !usbHost->clientHandle) return false;
    if (!input_task_stopped) input_task_stopped = xSemaphoreCreateBinary();
    if (!input_task_stopped) return false;
    input_task_running = true;
    if (xTaskCreatePinnedToCore(inputTask, "InputTask", INPUT_TASK_STACK_SIZE,
            nullptr, INPUT_TASK_PRIORITY, &input_task_handle, INPUT_TASK_CORE) != pdPASS) {
        input_task_running = false;
        return false;
    }
    Serial.println("[INPUT] USB mouse enabled in startup settings");
    return true;
}

uint32_t InputMouseReportCount(void) { return __atomic_load_n(&usb_mouse_reports,__ATOMIC_RELAXED); }
void InputUsbDescribe(char *out,size_t capacity)
{
    snprintf(out,capacity,"mouse=%u keyboard=%u reports=%lu rejected=%lu errors=%lu service_us=%lu poll_ms=%u",
        InputIsMouseConnected(),InputIsKeyboardConnected(),(unsigned long)InputMouseReportCount(),
        (unsigned long)__atomic_load_n(&usb_rejected_reports,__ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&usb_transfer_errors,__ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&usb_max_service_gap_us,__ATOMIC_RELAXED),INPUT_POLL_INTERVAL_MS);
}
bool InputUsbTrace(unsigned age,char *out,size_t capacity)
{
    UsbMouseTrace row;
    portENTER_CRITICAL(&usb_trace_lock);
    uint32_t count=usb_trace_sequence;
    bool valid=age<32 && age<count;
    if (valid) row=usb_trace[(count-1-age)%32];
    portEXIT_CRITICAL(&usb_trace_lock);
    if (!valid) return false;
    char encoded[33];const char *digits="0123456789abcdef";
    unsigned bytes=row.size<16?row.size:16;
    for(unsigned i=0;i<bytes;++i){encoded[2*i]=digits[row.raw[i]>>4];encoded[2*i+1]=digits[row.raw[i]&15];}
    encoded[2*bytes]=0;
    snprintf(out,capacity,"seq=%lu ms=%lu interface=%u ep=%u bytes=%u accepted=%u buttons=%u dx=%d dy=%d wheel=%d raw=%s",
        (unsigned long)row.sequence,(unsigned long)row.ms,row.interface_number,row.endpoint,row.size,
        row.accepted,row.buttons,row.dx,row.dy,row.wheel,encoded);
    return true;
}
void InputUsbResetDiagnostics(void)
{
    __atomic_store_n(&usb_mouse_reports,0,__ATOMIC_RELAXED);
    __atomic_store_n(&usb_rejected_reports,0,__ATOMIC_RELAXED);
    __atomic_store_n(&usb_transfer_errors,0,__ATOMIC_RELAXED);
    __atomic_store_n(&usb_max_service_gap_us,0,__ATOMIC_RELAXED);
    portENTER_CRITICAL(&usb_trace_lock);usb_trace_sequence=0;portEXIT_CRITICAL(&usb_trace_lock);
}

void InputExit(void)
{
    Serial.println("[INPUT] Shutting down input subsystem");
    
    if (input_task_handle != NULL) {
        input_task_running = false;
        if (input_task_stopped == NULL ||
            xSemaphoreTake(input_task_stopped, pdMS_TO_TICKS(5000)) != pdTRUE) {
            /* Do not force-delete a task that may own Wire/USB state. Leaving
             * its resources allocated is safer than tearing them out from
             * underneath it; this path indicates a deeper driver deadlock. */
            Serial.println("[INPUT] ERROR: input task did not stop; resources left intact");
            return;
        }
    }

    /* Release any held overlay keys / mouse state. */
    BirthdayGift_Close();
    TouchOverlay_Shutdown();

    InputAutomationReleaseAll();

    releaseAllTab5Bindings();
    tab5_keyboard_connected = false;
    BoardKeyboard_Exit();

    // Release any injected right-click Control key
    if (right_click_ctrl_injected) {
        releaseAdbKey(0x36);
        right_click_ctrl_injected = false;
    }
    
    if (usbHost != NULL) {
        usbHost->releaseAllKeyboardClaims();
        delete usbHost;
        usbHost = NULL;
    }
    if (input_task_stopped != NULL) {
        vSemaphoreDelete(input_task_stopped);
        input_task_stopped = NULL;
    }
}

void InputPoll(void)
{
    processTouchInput();
    
    if (usbHost != NULL) {
        usbHost->task();
    }
    
    updateKeyboardLEDs();
}

void InputSetScreenSize(int width, int height)
{
    mac_screen_width = width;
    mac_screen_height = height;
    Serial.printf("[INPUT] Mac screen size set to: %dx%d\n", width, height);
}

void InputSetTouchEnabled(bool enabled)
{
    touch_enabled = enabled;
    TouchOverlay_SetTouchEnabled(enabled);
}

void InputSetKeyboardEnabled(bool enabled)
{
    if (!enabled && keyboard_enabled && usbHost != NULL) {
        /* last_kb_report is the ownership snapshot for each USB slot. Clear
         * every claim before reports are ignored so a later disconnect cannot
         * decrement a key owned by the Tab5 keyboard or touch overlay. */
        usbHost->releaseAllKeyboardClaims();
    }
    keyboard_enabled = enabled;
}

bool InputIsKeyboardConnected(void)
{
    return usb_keyboard_connected || tab5_keyboard_connected;
}

bool InputIsMouseConnected(void)
{
    return mouse_connected;
}

// ============================================================================
// Legacy functions (kept for compatibility, now handled via USB Host callbacks)
// ============================================================================

void InputProcessKeyboardReport(const uint8_t *report, int length)
{
    (void)report;
    (void)length;
}

void InputProcessMouseReport(const uint8_t *report, int length)
{
    (void)report;
    (void)length;
}
