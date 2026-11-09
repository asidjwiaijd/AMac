#pragma once

#include <stdint.h>

namespace Birthday {

constexpr int kWidth = 480, kHeight = 360;
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};
constexpr Rect kWindow{24, 28, 432, 304};
constexpr Rect kClose{433, 35, 14, 14};
constexpr Rect kMac{42, 280, 32, 32};
constexpr Rect kStar{403, 34, 20, 18};
constexpr Rect kCake{162, 116, 156, 116};
constexpr Rect kBack{186, 292, 108, 22};
constexpr Rect kToast{70, 303, 340, 44};
constexpr Rect kMemory{68, 141, 344, 34};
constexpr Rect kTerminalButton{164, 215, 152, 26};

enum class Scene : uint8_t { Closed, Cake, Mac, Stars, Arcade, Letter, Settings, Terminal, About };
struct Frame {
    Scene scene = Scene::Closed;
    Scene back = Scene::Cake;
    uint8_t found = 0;
    bool toast = false, wished = false, pointer = false;
    int16_t mouse_x = kWidth / 2, mouse_y = kHeight / 2;
    uint32_t elapsed = 0;
    char command[40] = {}, last_command[40] = {};
    uint8_t terminal_output = 0;
};

// Platform-independent state machine. All timing is unsigned elapsed time so
// input sequences, holds and the invitation also work across millis() wrap.
class Gift {
public:
    void reset(uint32_t now);
    void launch(uint8_t entry, uint32_t now);
    bool key(uint8_t code, bool down, uint32_t now); // true: belongs to the gift
    bool move(int x, int y, bool relative, uint32_t now);
    bool button(uint8_t button, bool down, uint32_t now);
    void tick(uint32_t now);
    void close();
    bool open() const { return frame_.scene != Scene::Closed; }
    Frame frame(uint32_t now) const;
private:
    Frame frame_;
    bool keys_[128] = {}, buttons_[3] = {};
    bool initialized_ = false, ever_opened_ = false, holding_star_ = false;
    uint8_t konami_ = 0, year_ = 0, mac_clicks_ = 0;
    uint32_t born_ = 0, scene_at_ = 0, sequence_at_ = 0;
    uint32_t star_at_ = 0, mac_at_ = 0;
    int press_x_ = 0, press_y_ = 0;
    void show(Scene scene, uint32_t now);
    void terminalKey(uint8_t code, uint32_t now);
};

// Composites into a clipped RGB565 tile. Never writes guest memory. The same
// painter is used by firmware and the host previews/regression checks.
bool covers(const Frame &frame, int x, int y, int w, int h);
void composite(const Frame &frame, int x, int y, int w, int h, uint16_t *pixels);

} // namespace Birthday
