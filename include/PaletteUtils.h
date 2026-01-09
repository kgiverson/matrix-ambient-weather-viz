#pragma once

#include <Arduino.h>

namespace PaletteUtils {

// Shared 16-color base palette (from COLOR.md intent)
const uint8_t kBasePaletteRgb[16][3] = {
  {0, 24, 40},   {0, 56, 96},   {0, 96, 120},  {0, 140, 120},
  {0, 180, 100}, {0, 200, 40},  {40, 220, 40}, {80, 220, 60},
  {120, 200, 60}, {160, 180, 80}, {200, 140, 60}, {220, 100, 40},
  {200, 80, 80}, {160, 60, 120}, {120, 40, 140}, {80, 24, 120},
};

inline uint8_t clampU8(int value) {
  return (uint8_t)(value < 0 ? 0 : (value > 255 ? 255 : value));
}

inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Helper to interpolate between two RGB colors
inline void lerpRgb(const uint8_t a[3], const uint8_t b[3], float t, uint8_t out[3]) {
  out[0] = clampU8((int)(a[0] + (b[0] - a[0]) * t));
  out[1] = clampU8((int)(a[1] + (b[1] - a[1]) * t));
  out[2] = clampU8((int)(a[2] + (b[2] - a[2]) * t));
}

} // namespace PaletteUtils
