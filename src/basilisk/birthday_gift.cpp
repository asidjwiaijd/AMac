#if defined(BOARD_AMAC_P4) || defined(BIRTHDAY_HOST_TEST)
#include "birthday_gift.h"
#include <algorithm>

namespace Birthday {

void Gift::reset(uint32_t now)
{
    *this = Gift{};
    initialized_ = true;
    born_ = now;
}

void Gift::show(Scene scene, uint32_t now)
{
    frame_.scene = scene;
    frame_.found |= uint8_t(1u << (unsigned(scene) - 1));
    scene_at_ = now;
    ever_opened_ = true;
    holding_star_ = false;
    konami_ = year_ = mac_clicks_ = 0;
}

void Gift::close()
{
    frame_.scene = Scene::Closed;
    holding_star_ = false;
    konami_ = year_ = mac_clicks_ = 0;
}

bool Gift::key(uint8_t code, bool down, uint32_t now)
{
    if (!initialized_ || code >= 128) return false;
    const bool was_down = keys_[code];
    keys_[code] = down;
    const bool captured = open();
    if (!down || was_down) return captured;

    // Ctrl + Option/Alt + G. The broker releases any guest-held modifiers
    // when this opens, and suppresses their eventual physical key-up events.
    if (code == 0x05 && keys_[0x36] && keys_[0x3a]) {
        if (open()) close();
        else show(Scene::Cake, now);
        return true;
    }
    if (!open()) return false;
    if (code == 0x35) { // Escape: return to the cake, then back to the Mac.
        if (frame_.scene == Scene::Cake) close();
        else show(Scene::Cake, now);
        return true;
    }
    if (frame_.scene != Scene::Cake) {
        if (code == 0x24) show(Scene::Cake, now);
        return true;
    }
    if (code == 0x31) frame_.wished = true;

    if (now - sequence_at_ > 4000) konami_ = year_ = 0;
    sequence_at_ = now;
    // Up Up Down Down Left Right Left Right B A. Support both the classic
    // extended-keyboard arrows and the legacy ADB arrow codes.
    uint8_t arrow = code;
    if (code >= 0x7b && code <= 0x7e) arrow = code - 0x40;
    static const uint8_t combo[] = {0x3e,0x3e,0x3d,0x3d,0x3b,0x3c,0x3b,0x3c,0x0b,0x00};
    if (arrow == combo[konami_]) {
        if (++konami_ == sizeof(combo)) { show(Scene::Arcade, now); return true; }
    } else konami_ = arrow == combo[0] ? 1 : 0;

    static const uint8_t year[] = {0x12,0x19,0x1c,0x15,0x24}; // 1984 Return
    if (code == year[year_]) {
        if (++year_ == sizeof(year)) { show(Scene::Letter, now); return true; }
    } else year_ = code == year[0] ? 1 : 0;
    return true;
}

bool Gift::move(int x, int y, bool relative, uint32_t)
{
    if (!open()) return false;
    const int64_t nx = relative ? int64_t(frame_.mouse_x) + x : x;
    const int64_t ny = relative ? int64_t(frame_.mouse_y) + y : y;
    frame_.mouse_x = std::clamp<int64_t>(nx, 0, kWidth - 1);
    frame_.mouse_y = std::clamp<int64_t>(ny, 0, kHeight - 1);
    frame_.pointer = true;
    if (!kStar.contains(frame_.mouse_x, frame_.mouse_y)) holding_star_ = false;
    return true;
}

bool Gift::button(uint8_t button, bool down, uint32_t now)
{
    if (button >= 3) return false;
    const bool owned = buttons_[button];
    const bool captured = open() || owned;
    buttons_[button] = open() && down;
    if (!open() || button != 0 || down == owned) return captured;
    const int x = frame_.mouse_x, y = frame_.mouse_y;
    if (down) {
        press_x_ = x; press_y_ = y;
        holding_star_ = frame_.scene == Scene::Cake && kStar.contains(x, y);
        star_at_ = now;
        return true;
    }
    holding_star_ = false;
    auto click = [&](Rect r) { return r.contains(x, y) && r.contains(press_x_, press_y_); };
    if (click(kClose)) close();
    else if (frame_.scene != Scene::Cake && click(kBack)) show(Scene::Cake, now);
    else if (frame_.scene == Scene::Cake && click(kCake)) frame_.wished = true;
    else if (frame_.scene == Scene::Cake && click(kMac)) {
        mac_clicks_ = now - mac_at_ <= 1500 ? mac_clicks_ + 1 : 1;
        mac_at_ = now;
        if (mac_clicks_ == 5) show(Scene::Mac, now);
    } else mac_clicks_ = 0;
    return true;
}

void Gift::tick(uint32_t now)
{
    if (holding_star_ && buttons_[0] && now - star_at_ >= 3000)
        show(Scene::Stars, now);
}

Frame Gift::frame(uint32_t now) const
{
    Frame f = frame_;
    f.elapsed = now - scene_at_;
    const uint32_t age = now - born_;
    f.toast = initialized_ && !ever_opened_ && !open() && age >= 30000 && age < 45000;
    return f;
}
} // namespace Birthday
#endif
