#pragma once
#include <stdint.h>

// Send Finder's FNDR/shut Apple event. Call from a guest idle EMUL_OP so the
// interpreter restores the caller's registers and the Memory Manager is available.
// Applications get their ordinary save/cancel prompts before Mac PowerOff.
int16_t MacRequestShutdown();
