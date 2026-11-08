#if defined(BOARD_AMAC_P4)
#include "birthday_gift.h"
#include "birthday_gift_runtime.h"
#include "freertos/FreeRTOS.h"
#ifdef BIRTHDAY_HOST_TEST
#include "freertos/task.h"
#else
#include <Arduino.h>
#endif

namespace {
portMUX_TYPE gift_lock = portMUX_INITIALIZER_UNLOCKED;
Birthday::Gift gift;
Birthday::Frame shown;
bool sleeping = false;
}

void BirthdayGift_Init()
{
    portENTER_CRITICAL(&gift_lock);
    gift.reset(millis());
    sleeping = false;
    portEXIT_CRITICAL(&gift_lock);
}
void BirthdayGift_SetSleeping(bool value)
{
    portENTER_CRITICAL(&gift_lock);
    sleeping = value;
    portEXIT_CRITICAL(&gift_lock);
}
void BirthdayGift_Close()
{
    portENTER_CRITICAL(&gift_lock);
    gift.close();
    portEXIT_CRITICAL(&gift_lock);
}
bool BirthdayGift_Open()
{
    portENTER_CRITICAL(&gift_lock);
    const bool open = gift.open();
    portEXIT_CRITICAL(&gift_lock);
    return open;
}
bool BirthdayGift_Key(uint8_t code, bool down)
{
    portENTER_CRITICAL(&gift_lock);
    const bool consumed = gift.key(code, down, millis());
    portEXIT_CRITICAL(&gift_lock);
    return consumed;
}
bool BirthdayGift_MouseMove(int x, int y, bool relative)
{
    portENTER_CRITICAL(&gift_lock);
    const bool consumed = gift.move(x, y, relative, millis());
    portEXIT_CRITICAL(&gift_lock);
    return consumed;
}
bool BirthdayGift_MouseButton(uint8_t button, bool down)
{
    portENTER_CRITICAL(&gift_lock);
    const bool consumed = gift.button(button, down, millis());
    portEXIT_CRITICAL(&gift_lock);
    return consumed;
}
bool BirthdayGift_BeginFrame()
{
    const bool was_visible = shown.scene != Birthday::Scene::Closed || shown.toast;
    portENTER_CRITICAL(&gift_lock);
    if (sleeping) shown = Birthday::Frame{};
    else {
        gift.tick(millis());
        shown = gift.frame(millis());
    }
    portEXIT_CRITICAL(&gift_lock);
    return was_visible || shown.scene != Birthday::Scene::Closed || shown.toast;
}
bool BirthdayGift_TileCovered(int x, int y, int w, int h)
{
    return Birthday::covers(shown, x, y, w, h);
}
void BirthdayGift_CompositeTile(int x, int y, int w, int h, uint16_t *pixels)
{
    Birthday::composite(shown, x, y, w, h, pixels);
}
#endif
