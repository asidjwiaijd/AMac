#pragma once
#include <stddef.h>

// Main-task entry points. Network callbacks only queue samples; no guest or
// flash state is accessed from the lwIP task.
void MacNetworkTimePoll();
void MacNetworkTimeDescribe(char *out, size_t capacity);
