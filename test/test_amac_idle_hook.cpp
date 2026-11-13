#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <arpa/inet.h>
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
constexpr uint16 M68K_EMUL_OP_IDLE_TIME = 0x7137, M68K_NOP = 0x4e71;
static bool idlewait;
static unsigned flushed, shutdown_polls, yields;
static bool events;
static bool PrefsFindBool(const char *key) { assert(!strcmp(key,"idlewait"));return idlewait; }
static void FlushCodeCache(void *, unsigned size) { assert(size==4);++flushed; }
static void LowBatteryGuestIdle() { ++shutdown_polls; }
static uint32 ReadMacInt32(uint32 address) {
    if(address==0x14c) return events ? 1 : 0;
    assert(address==0x2b6);return 0x123456;
}
static void idle_wait() { ++yields; }
#define D(statement) do {} while(0)

// INSERT_PATCH

struct M68kRegisters { uint32 a[8]={}; };
static void idle_opcode(M68kRegisters *r) {
    switch(M68K_EMUL_OP_IDLE_TIME) {
// INSERT_OPCODE
    }
}

int main() {
    // System resource: SynchIdleTime implementation before the idle selector.
    alignas(2) uint8 resource[512]={};
    const uint8 entry[]={0x20,0x78,0x02,0xb6,0x41,0xe8,0x00,0x80};
    const uint8 selector[]={0x70,0x03,0xa0,0x9f};
    for(bool enabled : {false,true}) {
        memset(resource,0,sizeof(resource));
        memcpy(resource+160,entry,sizeof(entry));
        memcpy(resource+224,selector,sizeof(selector));
        idlewait=enabled;flushed=0;
        patch_idle_time(resource,sizeof(resource));
#if defined(BOARD_AMAC_P4)
        const bool installed=true;
#else
        const bool installed=enabled;
#endif
        assert(flushed==(installed ? 1u : 0u));
        if(installed) {
            assert(resource[160]==0x71 && resource[161]==0x37);
            assert(resource[162]==0x4e && resource[163]==0x71);
        } else assert(!memcmp(resource+160,entry,sizeof(entry)));
        // The callback does not enable the optional CPU yield as a side effect.
        shutdown_polls=yields=0;events=false;
        M68kRegisters r;idle_opcode(&r);
        assert(r.a[0]==0x123456 && yields==(enabled ? 1u : 0u));
#if defined(BOARD_AMAC_P4)
        assert(shutdown_polls==1);
#else
        assert(shutdown_polls==0);
#endif
        events=true;idle_opcode(&r);
        assert(yields==(enabled ? 1u : 0u));
    }
    // Malformed/short resource must not read before or beyond its buffer.
    idlewait=true;
    for(unsigned size=0;size<=sizeof(resource);++size) {
        memset(resource,0,sizeof(resource));
        if(size>=sizeof(selector)) memcpy(resource,selector,sizeof(selector));
        flushed=0;patch_idle_time(resource,size);assert(flushed==0);
    }
    puts("PASS: aMac low-voltage shutdown hook without idlewait; other boards and idle yield preserved");
}
