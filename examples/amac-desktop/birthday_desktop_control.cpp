// Test/development lifecycle control, executed at the guest's safe idle hook.
// Reuse the Finder shutdown implementation already validated in both Mac OSes.
#include "sysdeps.h"
#include "macos_util.h"
#include "mac_shutdown.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>

void AmacBirthdayIdle()
{
    static uint32_t checked=0;
    const uint32_t now=SDL_GetTicks();
    if (now-checked<500 || !HasMacStarted()) return;
    checked=now;
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