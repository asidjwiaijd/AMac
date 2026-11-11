#pragma once
#include "hid_mouse_channel.h"

// USB SDK and HID types must be included by the caller.
#define MAX_USB_DEVICES 4
#define MAX_TRANSFERS_PER_DEVICE 8
#define MAX_INTERFACES_PER_DEVICE 8
#define MAX_ENDPOINTS 17

struct EndpointInfo {
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t interface_number;
    HidMouseChannel mouse;
};

struct UsbDeviceSlot {
    bool active;
    bool disconnecting;
    uint32_t generation;
    usb_device_handle_t handle;
    bool has_keyboard;
    bool has_mouse;
    usb_transfer_t *transfers[MAX_TRANSFERS_PER_DEVICE];
    bool pending[MAX_TRANSFERS_PER_DEVICE];
    uint32_t retry_at[MAX_TRANSFERS_PER_DEVICE];
    bool recover_pipe[MAX_TRANSFERS_PER_DEVICE];
    unsigned control_pending;
    uint8_t transfer_count;
    uint8_t interfaces[MAX_INTERFACES_PER_DEVICE];
    uint8_t interface_count;
    EndpointInfo endpoint_info[MAX_ENDPOINTS];
    hid_keyboard_report_t last_kb_report;
    bool ready;

};
