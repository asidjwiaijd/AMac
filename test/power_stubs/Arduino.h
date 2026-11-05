// Host stand-ins for the Arduino calls board_power.cpp makes.
#pragma once
#include <cstdint>
#include <cstdio>

extern uint32_t g_fake_millis;
inline uint32_t millis() { return g_fake_millis; }

struct FakeSerial {
    template <typename... Args> int printf(const char *, Args...) { return 0; }
    void println(const char * = "") {}
};
extern FakeSerial Serial;

constexpr int INPUT = 0, LOW = 0, HIGH = 1, ADC_11db = 3;
void pinMode(int pin, int mode);
void analogSetPinAttenuation(int pin, int attenuation);
uint32_t analogReadMilliVolts(int pin);
int digitalRead(int pin);
