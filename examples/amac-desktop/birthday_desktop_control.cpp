// Test/development lifecycle control, executed at the guest's safe idle hook.
// Reuse the Finder shutdown implementation already validated in both Mac OSes.
#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"
#include "macos_util.h"
#include "mac_shutdown.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>

namespace {
// Use the System 7 Process Manager, so tests exercise each real HFS application
// and its GetResource call rather than bypassing the guest launcher.
int16_t launchGift(unsigned entry)
{
    static const char *const paths[]={
        ":Desktop Folder:For You",
        ":System Folder:Control Panels:Birthday Settings",
        ":System Folder:Apple Menu Items:Pocket Terminal",
        ":System Folder:Apple Menu Items:About This Gift",
    };
    if (entry<1 || entry>4) return -50;
    M68kRegisters r={}; r.d[0]=384;
    Execute68kTrap(0xa71e,&r); // NewPtrSysClear
    const uint32 block=r.a[0];
    if (!block) return -108;
    const char *path=paths[entry-1];
    WriteMacInt8(block+128,std::strlen(path));
    for (unsigned i=0;path[i];++i) WriteMacInt8(block+129+i,path[i]);
    // FSMakeFSSpec(0, fsRtDirID, path, &spec), Pascal stack convention.
    const uint16 code[]={0x558f,0x4267,0x2f3c,0,2,
                         0x486a,128,0x486a,48,0x7001,0xaa52,0x301f,0x4e75};
    for (unsigned i=0;i<sizeof(code)/2;++i) WriteMacInt16(block+256+2*i,code[i]);
    FlushCodeCache(Mac2HostAddr(block+256),sizeof(code));
    r={}; r.a[2]=block; Execute68k(block+256,&r);
    int16_t result=static_cast<int16_t>(r.d[0]);
    if (!result) {
        // LaunchParamBlockRec, 68K two-byte packing (Processes.h).
        WriteMacInt16(block+6,0x4c43); // extendedBlock
        WriteMacInt32(block+8,32);     // extendedBlockLen
        WriteMacInt16(block+14,0x4800);// launchContinue | launchNoFileFlags
        WriteMacInt32(block+16,block+48);
        r={}; r.a[0]=block; Execute68kTrap(0xa9f2,&r);
        result=static_cast<int16_t>(r.d[0]);
    }
    r={}; r.a[0]=block; Execute68kTrap(0xa01f,&r);
    return result;
}
}

void AmacBirthdayIdle()
{
    static uint32_t checked=0;
    const uint32_t now=SDL_GetTicks();
    if (now-checked<500 || !HasMacStarted()) return;
    checked=now;
    FILE *launch=std::fopen("browser/launch.request","rb");
    if (launch) {
        char entry[4]={};
        const size_t size=std::fread(entry,1,sizeof(entry)-1,launch);
        std::fclose(launch);
        if (size==2 && entry[0]>='1' && entry[0]<='4' && entry[1]=='\n' &&
            !std::remove("browser/launch.request")) {
            const unsigned page=entry[0]-'0';
            const int16_t result=launchGift(page);
            std::fprintf(stderr,"[DESKTOP] LaunchApplication entry=%u result=%d\n",page,result);
        }
    }
    FILE *request=std::fopen("browser/shutdown.request","rb");
    if (!request) return;
    char command[32]={};
    const size_t size=std::fread(command,1,sizeof(command)-1,request);
    std::fclose(request);
    if (size!=9 || std::strcmp(command,"shutdown\n")) return;
    if (std::remove("browser/shutdown.request")) return;
    const int16_t result=MacRequestShutdown();
    std::fprintf(stderr,"[DESKTOP] Finder shutdown result=%d\n",result);
}
