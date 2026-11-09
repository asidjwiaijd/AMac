#pragma once
#include <stdint.h>

#if defined(BOARD_AMAC_P4)
void BirthdayGift_Init();
void BirthdayGift_Close();
void BirthdayGift_SetSleeping(bool sleeping);
bool BirthdayGift_Open();
bool BirthdayGift_Key(uint8_t code, bool down);
bool BirthdayGift_MouseMove(int x, int y, bool relative);
bool BirthdayGift_MouseButton(uint8_t button, bool down);
void BirthdayGift_Launch(uint8_t entry); // input broker owns the guest handoff
void BirthdayGift_OpenPage(uint8_t entry);
// Video task only: freezes a frame and returns whether overlay damage exists.
bool BirthdayGift_BeginFrame();
bool BirthdayGift_TileCovered(int x, int y, int w, int h);
void BirthdayGift_CompositeTile(int x, int y, int w, int h, uint16_t *pixels);
#else
inline void BirthdayGift_Init() {}
inline void BirthdayGift_Close() {}
inline void BirthdayGift_SetSleeping(bool) {}
inline bool BirthdayGift_Open() { return false; }
inline bool BirthdayGift_Key(uint8_t, bool) { return false; }
inline bool BirthdayGift_MouseMove(int, int, bool) { return false; }
inline bool BirthdayGift_MouseButton(uint8_t, bool) { return false; }
inline void BirthdayGift_Launch(uint8_t) {}
inline void BirthdayGift_OpenPage(uint8_t) {}
inline bool BirthdayGift_BeginFrame() { return false; }
inline bool BirthdayGift_TileCovered(int, int, int, int) { return false; }
inline void BirthdayGift_CompositeTile(int, int, int, int, uint16_t *) {}
#endif
