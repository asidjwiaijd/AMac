#include "birthday_bridge.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

static std::array<bool,128> keys{};
static std::array<bool,3> buttons{};
void ADBKeyDown(int code) { assert(!keys[code]); keys[code]=true; }
void ADBKeyUp(int code) { assert(keys[code]); keys[code]=false; }
void ADBMouseDown(int button) { assert(!buttons[button]); buttons[button]=true; }
void ADBMouseUp(int button) { assert(buttons[button]); buttons[button]=false; }

static void launch(uint8_t entry) {
    // vCheckLoad may report a padded allocation rather than resource length.
    const uint8_t code[]={'a','M','a','c','B','d','a','y',1,entry,0,0,0,0};
    AmacBirthdayResource(0x62444159,128,code,sizeof(code));
}
static void key(SDL_Keycode sym) {
    SDL_Event event{}; event.type=SDL_KEYDOWN; event.key.keysym.sym=sym;
    event.key.keysym.scancode=SDL_GetScancodeFromKey(sym);
    assert(AmacBirthdayEvent(event,false));
    event.type=SDL_KEYUP; assert(AmacBirthdayEvent(event,false));
}
static void command(const char *s) { for (;*s;++s) key(*s); key(SDLK_RETURN); }
static void click(int x,int y) {
    SDL_Event event{}; event.type=SDL_MOUSEBUTTONDOWN;
    event.button.button=SDL_BUTTON_LEFT; event.button.x=x; event.button.y=y;
    assert(AmacBirthdayEvent(event,false));
    event.type=SDL_MOUSEBUTTONUP; assert(AmacBirthdayEvent(event,false));
}
static std::vector<uint32_t> draw(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer,18,52,86,255); SDL_RenderClear(renderer);
    AmacBirthdayFrame(); assert(AmacBirthdayDraw(renderer));
    std::vector<uint32_t> pixels(480*360);
    assert(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),480*4)==0);
    return pixels;
}
static bool background(const std::vector<uint32_t> &pixels) {
    for (auto p:pixels) if (p!=0xff123456) return false;
    return true;
}
int main()
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    auto *window=SDL_CreateWindow("Birthday adapter test",0,0,480,360,SDL_WINDOW_HIDDEN); assert(window);
    auto *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE); assert(renderer);
    AmacBirthdayInit(); assert(background(draw(renderer)));
    AmacDesktopKeyDown(0x36); AmacDesktopKeyDown(0x3a); AmacDesktopMouseDown(0);
    launch(1);
    for (bool key:keys) assert(!key);
    for (bool button:buttons) assert(!button);
    auto cake=draw(renderer); assert(!background(cake));
    // Transparent corners/shadows retain the desktop exactly, not RGB565 copies.
    for (auto p:std::array<int,4>{0,28*480+459,335*480+24,359*480+479}) assert(cake[p]==0xff123456);
    assert(cake[28*480+24]==0xff202839); // original firmware's ink in RGB565
    key(SDLK_SPACE); auto wished=draw(renderer); assert(wished!=cake);
    key(SDLK_ESCAPE); assert(background(draw(renderer)));
    launch(2); auto settings=draw(renderer);
    for (int i=0;i<5;++i) click(200,158);
    assert(draw(renderer)!=settings); key(SDLK_ESCAPE);
    launch(3); auto terminal=draw(renderer);
    command("ls"); assert(draw(renderer)!=terminal);
    command("cat clue.txt"); command("wish"); auto stars=draw(renderer);
    key(SDLK_ESCAPE); assert(draw(renderer)!=stars);
    command("cat birthday.log"); command("1984"); auto letter=draw(renderer);
    assert(letter!=stars); key(SDLK_ESCAPE); command("exit"); assert(background(draw(renderer)));
    launch(4); auto about=draw(renderer);
    for (auto sym:std::array<SDL_Keycode,10>{SDLK_UP,SDLK_UP,SDLK_DOWN,SDLK_DOWN,SDLK_LEFT,
        SDLK_RIGHT,SDLK_LEFT,SDLK_RIGHT,SDLK_b,SDLK_a}) key(sym);
    assert(draw(renderer)!=about);
    SDL_Event lost{}; lost.type=SDL_WINDOWEVENT; lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    AmacBirthdayEvent(lost,false); assert(background(draw(renderer)));
    AmacDesktopKeyUp(0x36); AmacDesktopMouseUp(0); // ignored orphan releases
    for (bool key:keys) assert(!key);
    for (bool button:buttons) assert(!button);
    AmacBirthdayDestroy(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    puts("PASS: actual SDL2 adapter, padded guest entry, alpha, all page inputs, focus and held-key handoff");
}
