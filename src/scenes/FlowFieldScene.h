#pragma once

#include <Arduino.h>

#include "Scene.h"

class FlowFieldScene : public Scene {
public:
  struct Vec2 {
    int16_t x;
    int16_t y;
  };

  struct WeatherParams {
    float temp_f;
    float wind_speed_mph;
    uint8_t cloud_cover_pct;
    uint8_t precip_prob_pct;
    bool valid;
  };

  FlowFieldScene();
  void begin(Adafruit_Protomatter &matrix) override;
  void update(uint32_t dt_ms) override;
  void render(Adafruit_Protomatter &matrix) override;
  void setWeather(const WeatherParams &params);

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
  static constexpr uint16_t kMinParticles = kParticleCount / 3;

  struct Particle {
    int32_t x_fp;
    int32_t y_fp;
    uint8_t age;
    uint8_t pad;
  };

  static uint32_t nextRand(uint32_t &state);
  static Vec2 direction(uint8_t idx);
  void updatePalette(uint8_t warmth);
  void applyPrecipBurst();

  uint32_t rng_;
  uint32_t field_accum_ms_;
  uint32_t drift_accum_ms_;
  uint16_t field_cursor_;
  uint16_t palette_[16];
  uint8_t palette_offset_;
  uint8_t drift_bias_;
  bool first_render_;
  uint16_t field_update_interval_ms_;
  uint8_t fade_factor_;
  uint16_t active_particles_;
  int16_t particle_speed_;
  uint32_t precip_accum_ms_;
  uint16_t precip_burst_interval_ms_;
  bool precip_burst_pending_;
  uint8_t last_temp_warm_;
  uint8_t last_cloud_;
  uint8_t last_wind_;
  uint8_t last_precip_;

  uint8_t field_[kFieldSize];
  Particle particles_[kParticleCount];
  WeatherParams weather_;
};
