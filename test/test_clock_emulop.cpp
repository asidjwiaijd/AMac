// Replay the Q650 ROM's ClkNoMem ABI through the production opcode handler
// and persistent clock backend. D1 carries the write byte in bits 16..23;
// D2 still contains the enclosing SetDateTime trap number (0x3a).
#include "mac_clock.h"
#include "nvs.h"
#include "esp_timer.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
constexpr uint16 M68K_EMUL_OP_CLKNOMEM = 0x7104;
constexpr int ROM_VERSION_32 = 32;
static int ROMVersion = ROM_VERSION_32;
static bool TwentyFourBitAddressing = false;
static uint8 XPRAM[256] = {};
struct M68kRegisters { uint32 d[8] = {}; };
#define D(statement) do {} while (0)

static void execute(M68kRegisters *r) {
    switch (M68K_EMUL_OP_CLKNOMEM) {
#include "clock_emulop_case.inc"
    }
}

static uint8 access(uint16 command, uint8 value = 0) {
    M68kRegisters r;
    r.d[1] = (uint32(value) << 16) | command;
    r.d[2] = 0x3a; // Captured ROM call path: not the outgoing data.
    execute(&r);
    assert(r.d[0] == 0);
    assert(r.d[1] == r.d[2]);
    return uint8(r.d[1]);
}

static uint16 extended(unsigned reg, bool read = false) {
    return uint16(0x38 | ((reg >> 5) & 7) | ((reg & 31) << 10) | (read ? 0x80 : 0));
}

int main() {
    const uint32 date = 0xe71cc900; // Distinct bytes, modern Mac timestamp.
    for (unsigned alias : {0u, 4u}) {
        uint32 old = TimerDateTime();
        for (unsigned i = 0; i < 4; ++i) {
            access(uint16(((i + alias) << 2) | 1), uint8(date >> (8 * i)));
            assert(TimerDateTime() == (i == 3 ? date : old));
        }
        for (unsigned i = 0; i < 8; ++i)
            assert(access(uint16((i << 2) | 0x81), 0xaa) == uint8(date >> (8 * (i & 3))));
    }
    MacClockSave();
    assert(test_commits == 1 && test_saved == date);
    test_now_us += 2000000;
    assert(TimerDateTime() == date + 2);

    // Time-zone/other PRAM writes use the same packed D1 input convention.
    for (unsigned reg = 0; reg < 256; ++reg) {
        uint8 value = uint8(reg ^ 0xa5);
        if (reg == 0x8a) value |= 5;
        access(extended(reg), value);
        assert(XPRAM[reg] == value);
        assert(access(extended(reg, true), 0xcc) == value);
    }
    for (unsigned reg : {8u, 9u, 10u, 11u, 16u, 31u}) {
        access(uint16(reg << 2), 0x96);
        assert(XPRAM[reg] == 0x96);
        assert(access(uint16((reg << 2) | 0x80), 0xcc) == 0x96);
    }
    std::puts("PASS: Q650 packed RTC/PRAM writes, read ABI and persistent clock");
}
