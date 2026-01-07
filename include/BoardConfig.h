#pragma once

#include <Adafruit_Protomatter.h>

// Matrix Portal M4 + 64x32 HUB75 (known-good pins from prior project).
extern uint8_t kMatrixRgbPins[];
extern uint8_t kMatrixAddrPins[];
constexpr uint8_t kMatrixClockPin = 14;
constexpr uint8_t kMatrixLatchPin = 15;
constexpr uint8_t kMatrixOePin = 16;

constexpr uint16_t kMatrixWidth = 64;
constexpr uint8_t kMatrixBitplanes = 4;
constexpr uint8_t kMatrixChains = 1;
constexpr uint8_t kMatrixAddrLines = 4; // A-D for 32px tall panels
constexpr bool kMatrixDoubleBuffer = true;

extern Adafruit_Protomatter matrix;
