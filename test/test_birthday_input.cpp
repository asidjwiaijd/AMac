#include <cstring>
#include <cstdio>
#include "video_test_stubs.h"
#include "birthday_gift_runtime.h"
static int mac_screen_width=480, mac_screen_height=360;
static bool keys[128]={}, buttons[3]={};
static int guest_moves=0, activity=0;
static void ADBKeyDown(int code) { assert(!keys[code]); keys[code]=true; }
static void ADBKeyUp(int code) { assert(keys[code]); keys[code]=false; }
static void ADBSetRelMouseMode(bool) {}
static void ADBMouseMoved(int,int) { ++guest_moves; }
static void ADBMouseDown(int button) { assert(!buttons[button]); buttons[button]=true; }
static void ADBMouseUp(int button) { assert(buttons[button]); buttons[button]=false; }
static void PowerManagerNoteActivity() { ++activity; }
#include "../src/basilisk/input_shared_state.inc"
static void tap(uint8_t code) { InputAutomationKey(code,true); InputAutomationKey(code,false); }
int main()
{
    BirthdayGift_Init(); resetInputClaims();
    InputKeyDown(0x36); InputAutomationKey(0x36,true); // two owners
    InputKeyDown(0x3a); InputKeyDown(0x00); routeMouseButton(0,true);
    assert(keys[0x00] && keys[0x36] && keys[0x3a] && buttons[0]);
    InputAutomationKey(0x05,true);
    assert(BirthdayGift_Open());
    for (bool key:keys) assert(!key);
    for (bool button:buttons) assert(!button);
    assert(adb_key_claims[0x36]==2 && adb_key_claims[0x00]==1);
    InputAutomationReleaseAll();
    InputKeyUp(0x36); InputKeyUp(0x3a); InputKeyUp(0x00);
    routeMouseButton(0,false); // no orphan release after handover
    tap(0x31); InputAutomationMouseMove(240,180,false);
    assert(guest_moves==0 && activity>=3);
    // Esc closes; a key held at close must not leak its eventual key-up.
    InputKeyDown(0x00); tap(0x35); assert(!BirthdayGift_Open()); InputKeyUp(0x00);
    InputAutomationMouseMove(2,3,true); assert(guest_moves==1);
    InputKeyDown(0x00); assert(keys[0x00]); InputKeyUp(0x00); assert(!keys[0x00]);
    // Hotkey toggles closed while all three keys are still physically held.
    InputKeyDown(0x36); InputKeyDown(0x3a); InputKeyDown(0x05);
    assert(BirthdayGift_Open()); InputKeyUp(0x05); InputKeyDown(0x05);
    assert(!BirthdayGift_Open()); InputKeyUp(0x05); InputKeyUp(0x36); InputKeyUp(0x3a);
    for (bool key:keys) assert(!key);
    // Physical and serial mouse paths share capture and hold ownership.
    InputKeyDown(0x36); InputKeyDown(0x3a); tap(0x05);
    InputKeyUp(0x36); InputKeyUp(0x3a);
    InputAutomationMouseMove(440,42,false); InputAutomationMouseButton(0,true);
    InputAutomationMouseButton(0,false); assert(!BirthdayGift_Open());
    InputAutomationMouseButton(0,true); assert(buttons[0]);
    InputAutomationReleaseAll(); assert(!buttons[0]);
    InputKeyDown(0x38); routeMouseButton(0,true);
    BirthdayGift_Launch(2); assert(BirthdayGift_Open() && !keys[0x38] && !buttons[0]);
    InputKeyUp(0x38); routeMouseButton(0,false); tap(0x35);
    assert(!BirthdayGift_Open());
    for (int count:adb_key_claims) assert(count==0);
    puts("PASS: production input broker captures gifts, releases held guest input and resumes without stuck keys/buttons");
}
