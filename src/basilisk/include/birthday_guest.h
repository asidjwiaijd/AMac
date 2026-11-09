#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Native Finder launchers explicitly load bDAY/128, then exit normally.
// The legacy CODE marker remains supported. A complete signature is required.
inline uint8_t BirthdayGuestEntry(uint32_t type, int16_t id, const uint8_t *data, size_t size)
{
    static const uint8_t prefix[] = {0,0,0,1,0xa9,0xf4,'a','M','a','c','B','d','a','y',1};
    static const uint8_t marker[] = {'a','M','a','c','B','d','a','y',1};
    if (type==0x62444159 && id==128 && data && size>=sizeof(marker)+1 &&
        memcmp(data,marker,sizeof(marker))==0) {
        const uint8_t entry=data[sizeof(marker)];
        return entry>=1 && entry<=4 ? entry:0;
    }
    // The ROM's vCheckLoad passes a heap-block size, which can include padding.
    if (type != 0x434f4445 || id != 1 || !data || size < sizeof(prefix)+1 ||
        memcmp(data,prefix,sizeof(prefix)) != 0) return 0;
    const uint8_t entry=data[sizeof(prefix)];
    return entry>=1 && entry<=4 ? entry : 0;
}
