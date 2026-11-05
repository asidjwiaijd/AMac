#if defined(BOARD_AMAC_P4) || defined(MAC_SHUTDOWN_HOST_TEST)
#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"
#include "mac_shutdown.h"

namespace {
// A2 points to three AEDesc records (0, 8, 16) and the Finder signature (24).
// Pascal Toolbox functions return their OSErr in a word reserved on the stack.
const uint16 create_address[] = {
    0x558f,                         // subq.l #2,sp (OSErr)
    0x2f3c, 0x7369, 0x676e,         // move.l #'sign',-(sp)
    0x486a, 24,                     // pea 24(a2) (Finder application signature)
    0x2f3c, 0, 4,                   // move.l #4,-(sp)
    0x2f0a,                         // move.l a2,-(sp)
    0x303c, 0x0825, 0xa816,         // AECreateDesc
    0x301f, 0x4e75,                 // move.w (sp)+,d0; rts
};
const uint16 create_event[] = {
    0x558f,
    0x2f3c, 0x464e, 0x4452,         // event class 'FNDR'
    0x2f3c, 0x7368, 0x7574,         // event ID 'shut'
    0x2f0a,                         // target address descriptor
    0x3f3c, 0xffff,                 // kAutoGenerateReturnID
    0x42a7,                         // clr.l -(sp), kAnyTransactionID
    0x486a, 8,
    0x303c, 0x0b14, 0xa816,         // AECreateAppleEvent
    0x301f, 0x4e75,
};
const uint16 send_event[] = {
    0x558f,
    0x486a, 8, 0x486a, 16,         // event, reply
    0x2f3c, 0, 0x0071,             // no reply, always interact, can switch layer
    0x3f3c, 0,                     // normal priority
    0x2f3c, 0xffff, 0xffff,         // default timeout
    0x42a7, 0x42a7,                // idleProc, filterProc
    0x303c, 0x0d17, 0xa816,         // AESend
    0x301f, 0x4e75,
};
const uint16 dispose_desc[] = {
    0x558f, 0x2f08,                 // OSErr, move.l a0,-(sp)
    0x303c, 0x0204, 0xa816,         // AEDisposeDesc
    0x301f, 0x4e75,
};

int16 run(uint32 block, const uint16 *code, size_t words, uint32 descriptor = 0)
{
    for (size_t i = 0; i < words; ++i) WriteMacInt16(block + 32 + i * 2, code[i]);
    FlushCodeCache(Mac2HostAddr(block + 32), words * 2);
    M68kRegisters r = {};
    r.a[2] = block;
    r.a[0] = descriptor;
    Execute68k(block + 32, &r);
    return (int16)r.d[0];
}
} // namespace

int16_t MacRequestShutdown()
{
    M68kRegisters r = {};
    r.d[0] = 160;
    Execute68kTrap(0xa71e, &r);       // NewPtrSysClear
    const uint32 block = r.a[0];
    if (!block) return -108;         // memFullErr
    WriteMacInt32(block + 24, 0x4d414353); // Finder's application signature is 'MACS'.
    int16 err = run(block, create_address, sizeof(create_address) / 2);
    if (!err) err = run(block, create_event, sizeof(create_event) / 2);
    if (!err) err = run(block, send_event, sizeof(send_event) / 2);
    for (uint32 offset = 0; offset < 24; offset += 8) {
        if (ReadMacInt32(block + offset + 4))
            run(block, dispose_desc, sizeof(dispose_desc) / 2, block + offset);
    }
    r = {};
    r.a[0] = block;
    Execute68kTrap(0xa01f, &r);       // DisposePtr
    return err;
}
#endif
