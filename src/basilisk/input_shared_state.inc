// Shared by physical input and serial automation. A recursive task mutex
// covers the claim transition AND its ADB event so another task cannot send
// key-up before the corresponding key-down. ADB can block; use no spinlock.
static StaticSemaphore_t s_input_state_storage;
static SemaphoreHandle_t inputStateMutex()
{
    static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutexStatic(&s_input_state_storage);
    return mutex;
}
struct InputStateGuard {
    InputStateGuard() { xSemaphoreTakeRecursive(inputStateMutex(), portMAX_DELAY); }
    ~InputStateGuard() { xSemaphoreGiveRecursive(inputStateMutex()); }
};

/* Physical-key claims from every input source, indexed by final Mac ADB
 * keycode. USB slots, the Tab5 Keyboard matrix, and synthetic right-click
 * Control each own an independent claim. ADB sees a down event only on the
 * first claim and an up event only when the last claim is released, so two
 * keyboards can hold the same key/modifier without releasing each other. */
DRAM_ATTR static uint8_t adb_key_claims[128] = {};
DRAM_ATTR static bool automation_keys_down[128] = {};
static bool automation_mouse_buttons[3] = {};

#if defined(BOARD_AMAC_P4)
static bool guest_keys_down[128] = {}, guest_mouse_down[3] = {};

// Keep physical ownership, but end every forwarded press before the gift
// takes input. Closing the gift never invents a new guest key-down: held
// keys must be released and pressed again, including the opening modifiers.
static void releaseGuestInputForGift()
{
    for (int code = 0; code < 128; ++code) {
        if (guest_keys_down[code]) ADBKeyUp(code);
        guest_keys_down[code] = false;
    }
    for (int button = 0; button < 3; ++button) {
        if (guest_mouse_down[button]) ADBMouseUp(button);
        guest_mouse_down[button] = false;
    }
}
void BirthdayGift_Launch(uint8_t entry)
{
    if (entry<1 || entry>4) return;
    InputStateGuard guard;
    releaseGuestInputForGift();
    BirthdayGift_OpenPage(entry);
    PowerManagerNoteActivity();
}
#endif

static void routeMouseMove(int x, int y, bool relative)
{
    InputStateGuard guard;
#if defined(BOARD_AMAC_P4)
    if (BirthdayGift_MouseMove(x, y, relative)) {
        if (!relative || x || y) PowerManagerNoteActivity();
        return;
    }
#endif
    ADBSetRelMouseMode(relative);
    ADBMouseMoved(x, y);
}

static void routeMouseButton(uint8_t button, bool pressed)
{
    InputStateGuard guard;
    if (button >= 3) return;
#if defined(BOARD_AMAC_P4)
    if (BirthdayGift_MouseButton(button, pressed)) {
        if (pressed) PowerManagerNoteActivity();
        return;
    }
    if (!pressed && !guest_mouse_down[button]) return;
    guest_mouse_down[button] = pressed;
#endif
    if (pressed) ADBMouseDown(button);
    else ADBMouseUp(button);
}

static void claimAdbKey(uint8_t mac_keycode)
{
    InputStateGuard guard;
    if (mac_keycode >= 0x80) return;
    uint8_t &count = adb_key_claims[mac_keycode];
    if (count == 0) {
#if defined(BOARD_AMAC_P4)
        const bool was_open = BirthdayGift_Open();
        const bool consumed = BirthdayGift_Key(mac_keycode, true);
        if (!was_open && BirthdayGift_Open()) releaseGuestInputForGift();
        if (consumed) PowerManagerNoteActivity();
        else {
            ADBKeyDown(mac_keycode);
            guest_keys_down[mac_keycode] = true;
        }
#else
        ADBKeyDown(mac_keycode);
#endif
    }
    if (count != 0xFF) {
        ++count;
    } else {
        Serial.printf("[INPUT] WARN: ADB key 0x%02X claim count saturated\n",
                      mac_keycode);
    }
}

static void releaseAdbKey(uint8_t mac_keycode)
{
    InputStateGuard guard;
    if (mac_keycode >= 0x80) return;
    uint8_t &count = adb_key_claims[mac_keycode];
    if (count == 0) {
        return;  // Ignore orphan releases from a reset/disconnected source.
    }
    --count;
    if (count == 0) {
#if defined(BOARD_AMAC_P4)
        BirthdayGift_Key(mac_keycode, false);
        if (guest_keys_down[mac_keycode]) ADBKeyUp(mac_keycode);
        guest_keys_down[mac_keycode] = false;
#else
        ADBKeyUp(mac_keycode);
#endif
    }
}

extern "C" void InputKeyDown(uint8_t mac_keycode)
{
    claimAdbKey(mac_keycode);
}

extern "C" void InputKeyUp(uint8_t mac_keycode)
{
    releaseAdbKey(mac_keycode);
}

extern "C" void InputAutomationKey(uint8_t mac_keycode, bool pressed)
{
    InputStateGuard guard;
    if (mac_keycode >= 0x80 || automation_keys_down[mac_keycode] == pressed) {
        return;
    }
    automation_keys_down[mac_keycode] = pressed;
    if (pressed) {
        claimAdbKey(mac_keycode);
    } else {
        releaseAdbKey(mac_keycode);
    }
}

extern "C" void InputAutomationMouseMove(int x, int y, bool relative)
{
    InputStateGuard guard;
    if (!relative) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= mac_screen_width) x = mac_screen_width - 1;
        if (y >= mac_screen_height) y = mac_screen_height - 1;
    }
    routeMouseMove(x, y, relative);
}

extern "C" void InputAutomationMouseButton(uint8_t button, bool pressed)
{
    InputStateGuard guard;
    if (button >= 3 || automation_mouse_buttons[button] == pressed) {
        return;
    }
    automation_mouse_buttons[button] = pressed;
    routeMouseButton(button, pressed);
}

extern "C" void InputAutomationReleaseAll(void)
{
    InputStateGuard guard;
    for (uint16_t code = 0; code < 128; ++code) {
        if (automation_keys_down[code]) {
            automation_keys_down[code] = false;
            releaseAdbKey((uint8_t)code);
        }
    }
    for (uint8_t button = 0; button < 3; ++button) {
        if (automation_mouse_buttons[button]) {
            automation_mouse_buttons[button] = false;
            routeMouseButton(button, false);
        }
    }
}

static bool controlKeyClaimed()
{
    InputStateGuard guard;
    return adb_key_claims[0x36] != 0;
}

static void resetInputClaims()
{
    InputStateGuard guard;
    memset(adb_key_claims, 0, sizeof(adb_key_claims));
    memset(automation_keys_down, 0, sizeof(automation_keys_down));
    memset(automation_mouse_buttons, 0, sizeof(automation_mouse_buttons));
#if defined(BOARD_AMAC_P4)
    memset(guest_keys_down, 0, sizeof(guest_keys_down));
    memset(guest_mouse_down, 0, sizeof(guest_mouse_down));
#endif
}
