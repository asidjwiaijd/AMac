#pragma once
#include "hid_descriptor.h"

// One channel per interrupt endpoint, bound to that endpoint's interface.
// Pending/unsupported descriptors never turn arbitrary vendor packets into
// coordinates. Report IDs are validated by the descriptor decoder.
struct HidMouseChannel {
    HidMouseLayout layout = {};
    bool described = false;
    uint8_t buttons = 0;

    bool decode(const uint8_t *data, size_t size, uint8_t *out_buttons,
                int16_t *x, int16_t *y, int16_t *wheel) const
    {
        return described && layout.valid &&
            HidDecodeMouseReport(data,size,&layout,out_buttons,x,y,wheel);
    }
};
