#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

using String = std::string;
constexpr int HIGH = 1, LOW = 0, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
uint32_t millis();
void delay(uint32_t ms);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
void pinMode(int pin, int mode);
struct SerialStub {
    void println(const char *) {}
    template<class... Args> void printf(const char *, Args...) {}
};
inline SerialStub Serial;
struct EspStub { uint64_t getEfuseMac() { return 0x3C0F02F967E0ULL; } };
inline EspStub ESP;
