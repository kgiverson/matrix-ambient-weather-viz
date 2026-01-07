#pragma once

#include <Arduino.h>

#include "Scene.h"

class FlowFieldScene : public Scene {
public:
  struct Vec2 {
    int16_t x;
    int16_t y;
  };

  FlowFieldScene();
  void begin(Adafruit_Protomatter &matrix) override;
  void update(uint32_t dt_ms) override;
  void render(Adafruit_Protomatter &matrix) override;

private:
  static constexpr uint8_t kFieldCols = 16;
  static constexpr uint8_t kFieldRows = 8;
  static constexpr uint16_t kFieldSize = kFieldCols * kFieldRows;
  static constexpr uint16_t kParticleCount = 240;
  static constexpr uint8_t kFixedShift = 8;
  static constexpr int32_t kFixedOne = 1 << kFixedShift;
  static constexpr int16_t kParticleSpeed = 40; // pixels per second
  static constexpr uint32_t kFieldUpdateIntervalMs = 40;
  static constexpr uint8_t kFadeFactor = 248; // 0-255
  static constexpr uint32_t kDriftIntervalMs = 80;

  struct Particle {
    int32_t x_fp;
    int32_t y_fp;
    uint8_t age;
    uint8_t pad;
  };

  static uint32_t nextRand(uint32_t &state);
  static Vec2 direction(uint8_t idx);

  uint32_t rng_;
  uint32_t field_accum_ms_;
  uint32_t drift_accum_ms_;
  uint16_t field_cursor_;
  uint16_t palette_[16];
  uint8_t palette_offset_;
  uint8_t drift_bias_;
  bool first_render_;

  uint8_t field_[kFieldSize];
  Particle particles_[kParticleCount];
};
