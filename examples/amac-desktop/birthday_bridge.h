#pragma once
#include <SDL.h>

void AmacBirthdayInit();
void AmacBirthdayDestroy();
bool AmacBirthdayEvent(const SDL_Event &event, bool relative);
bool AmacBirthdayFrame();
bool AmacBirthdayDraw(SDL_Renderer *renderer);
void AmacDesktopKeyDown(int code);
void AmacDesktopKeyUp(int code);
void AmacDesktopMouseDown(int button);
void AmacDesktopMouseUp(int button);
void AmacBirthdayResource(uint32_t type, int16_t id, const uint8_t *data, uint32_t size);
void AmacBirthdayIdle();
