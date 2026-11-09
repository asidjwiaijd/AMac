// SDL adapter for the firmware's unmodified birthday state machine and painter.
// The overlay is a separate texture: the emulated Mac keeps its own framebuffer.
#include "birthday_bridge.h"
#include "birthday_gift.h"
#include "birthday_guest.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <mutex>

// Stable C++ entry points exported by Basilisk II's adb.cpp.
extern void ADBKeyDown(int code);
extern void ADBKeyUp(int code);
extern void ADBMouseDown(int button);
extern void ADBMouseUp(int button);

namespace {
std::mutex input_lock;
Birthday::Gift gift;
Birthday::Frame frame;
std::array<bool,128> guest_keys{};
std::array<bool,3> guest_buttons{};
std::array<bool,SDL_NUM_SCANCODES> swallowed{};
SDL_Texture *texture = nullptr;

int giftCode(SDL_Keycode key)
{
    static const uint8_t letters[]={0x00,0x0b,0x08,0x02,0x0e,0x03,0x05,0x04,0x22,0x26,0x28,0x25,0x2e,
        0x2d,0x1f,0x23,0x0c,0x0f,0x01,0x11,0x20,0x09,0x0d,0x07,0x10,0x06};
    if (key>=SDLK_a && key<=SDLK_z) return letters[key-SDLK_a];
    static const uint8_t digits[]={0x1d,0x12,0x13,0x14,0x15,0x17,0x16,0x1a,0x1c,0x19};
    if (key>=SDLK_0 && key<=SDLK_9) return digits[key-SDLK_0];
    switch (key) {
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x36;
    case SDLK_LALT: case SDLK_RALT: return 0x3a;
    case SDLK_g: return 0x05;
    case SDLK_ESCAPE: return 0x35;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x24;
    case SDLK_SPACE: return 0x31;
    case SDLK_BACKSPACE: return 0x33;
    case SDLK_PERIOD: return 0x2f;
    case SDLK_UP: return 0x3e;
    case SDLK_DOWN: return 0x3d;
    case SDLK_LEFT: return 0x3b;
    case SDLK_RIGHT: return 0x3c;
    case SDLK_b: return 0x0b;
    case SDLK_a: return 0x00;
    case SDLK_1: return 0x12;
    case SDLK_9: return 0x19;
    case SDLK_8: return 0x1c;
    case SDLK_4: return 0x15;
    default: return 0x34; // unused Mac key: cancels any partial secret
    }
}

void releaseGuestInput()
{
    for (int code=0;code<128;++code) {
        if (guest_keys[code]) ADBKeyUp(code);
        guest_keys[code]=false;
    }
    for (int button=0;button<3;++button) {
        if (guest_buttons[button]) ADBMouseUp(button);
        guest_buttons[button]=false;
    }
}
}

void AmacBirthdayResource(uint32_t type, int16_t id, const uint8_t *data, uint32_t size)
{
    const uint8_t entry=BirthdayGuestEntry(type,id,data,size);
    if (!entry) return;
    std::lock_guard<std::mutex> lock(input_lock);
    releaseGuestInput();
    gift.launch(entry,SDL_GetTicks());
    std::fprintf(stderr,"[BIRTHDAY] Finder application entry=%u\n",entry);
}

void AmacBirthdayInit()
{
    std::lock_guard<std::mutex> lock(input_lock);
    gift.reset(SDL_GetTicks());
    guest_keys.fill(false); guest_buttons.fill(false); swallowed.fill(false);
}

void AmacBirthdayDestroy()
{
    if (texture) SDL_DestroyTexture(texture);
    texture=nullptr;
}

bool AmacBirthdayEvent(const SDL_Event &event, bool relative)
{
    std::lock_guard<std::mutex> lock(input_lock);
    const uint32_t now=SDL_GetTicks();
    if (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
        const bool down=event.type==SDL_KEYDOWN;
        const int scan=event.key.keysym.scancode;
        if (scan<0 || scan>=SDL_NUM_SCANCODES) return gift.open();
        const int code=giftCode(event.key.keysym.sym);
        const bool owned=swallowed[scan];
        const bool was_open=gift.open();
        const bool launch=down && !event.key.repeat && !was_open &&
            event.key.keysym.sym==SDLK_g && (event.key.keysym.mod & KMOD_CTRL) &&
            (event.key.keysym.mod & KMOD_ALT);
        if (launch) {
            gift.key(0x36,true,now); gift.key(0x3a,true,now);
            gift.key(0x05,true,now);
            releaseGuestInput(); swallowed[scan]=true;
            return true;
        }
        if (was_open || owned) {
            if (!event.key.repeat) gift.key(code,down,now);
            swallowed[scan]=down;
            return true;
        }
        // A launch modifier may be released after the overlay was closed.
        if (!down) gift.key(code,false,now);
        return false;
    }
    if (event.type==SDL_MOUSEMOTION) {
        return gift.move(relative ? event.motion.xrel:event.motion.x,
                         relative ? event.motion.yrel:event.motion.y,relative,now);
    }
    if (event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEBUTTONUP) {
        const int button=event.button.button==SDL_BUTTON_LEFT ? 0:
                         event.button.button==SDL_BUTTON_RIGHT ? 1:
                         event.button.button==SDL_BUTTON_MIDDLE ? 2:-1;
        const bool was_open=gift.open();
        if (was_open && !relative) gift.move(event.button.x,event.button.y,false,now);
        if (button<0) return was_open;
        return gift.button(button,event.type==SDL_MOUSEBUTTONDOWN,now);
    }
    if (event.type==SDL_MOUSEWHEEL) return gift.open();
    if (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
        // SDL can lose modifier releases when focus moves to another app.
        releaseGuestInput(); gift.close();
        for (int code=0;code<128;++code) gift.key(code,false,now);
        for (int button=0;button<3;++button) gift.button(button,false,now);
        swallowed.fill(false);
    }
    if (event.type==SDL_QUIT) gift.close(); // keep the normal Mac power-key path
    return false;
}

bool AmacBirthdayFrame()
{
    std::lock_guard<std::mutex> lock(input_lock);
    const bool old_visible=frame.scene!=Birthday::Scene::Closed || frame.toast;
    gift.tick(SDL_GetTicks());
    frame=gift.frame(SDL_GetTicks());
    return old_visible || frame.scene!=Birthday::Scene::Closed || frame.toast;
}

bool AmacBirthdayDraw(SDL_Renderer *renderer)
{
    if (frame.scene==Birthday::Scene::Closed && !frame.toast) return true;
    constexpr int w=Birthday::kWidth, h=Birthday::kHeight;
    // Two different backgrounds derive an exact transparency mask, including
    // black/white pixels. This also preserves the real desktop around shadows.
    static std::array<uint16_t,w*h> dark, light;
    static std::array<uint32_t,w*h> rgba;
    dark.fill(0); light.fill(0xffff);
    Birthday::composite(frame,0,0,w,h,dark.data());
    Birthday::composite(frame,0,0,w,h,light.data());
    for (int i=0;i<w*h;++i) {
        const auto c=dark[i];
        rgba[i]=dark[i]!=light[i] ? 0:
            0xff000000u | (((c>>11)&31)*255/31)<<16 |
            (((c>>5)&63)*255/63)<<8 | ((c&31)*255/31);
    }
    if (!texture) {
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
        if (!texture || SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND)!=0) {
            std::fprintf(stderr,"[BIRTHDAY] Texture: %s\n",SDL_GetError()); return false;
        }
    }
    SDL_Rect dest{0,0,w,h};
    return SDL_UpdateTexture(texture,nullptr,rgba.data(),w*4)==0 &&
           SDL_RenderCopy(renderer,texture,nullptr,&dest)==0;
}

void AmacDesktopKeyDown(int code)
{
    std::lock_guard<std::mutex> lock(input_lock);
    if (code<0 || code>=128 || guest_keys[code]) return;
    guest_keys[code]=true; ADBKeyDown(code);
}
void AmacDesktopKeyUp(int code)
{
    std::lock_guard<std::mutex> lock(input_lock);
    if (code<0 || code>=128 || !guest_keys[code]) return;
    guest_keys[code]=false; ADBKeyUp(code);
}
void AmacDesktopMouseDown(int button)
{
    std::lock_guard<std::mutex> lock(input_lock);
    if (button<0 || button>=3 || guest_buttons[button]) return;
    guest_buttons[button]=true; ADBMouseDown(button);
}
void AmacDesktopMouseUp(int button)
{
    std::lock_guard<std::mutex> lock(input_lock);
    if (button<0 || button>=3 || !guest_buttons[button]) return;
    guest_buttons[button]=false; ADBMouseUp(button);
}
