#pragma once

#include "Scene.h"

class ReactionDiffusionScene : public Scene {
public:
  ReactionDiffusionScene();
  virtual ~ReactionDiffusionScene();

  void begin(Adafruit_Protomatter &matrix) override;
  void update(uint32_t dt_ms) override;
  void render(Adafruit_Protomatter &matrix) override;
  void setWeather(const WeatherParams &params) override;

private:
  static constexpr int kWidth = 64;
  static constexpr int kHeight = 32;
  static constexpr int kGridSize = kWidth * kHeight;

  // Gray-Scott parameters (Default "Spots" / "Cells")
  // F=0.0545, k=0.0620 -> Coral / Brains
  // F=0.035, k=0.065 -> Spots
  float feed_;
  float kill_;
  float diff_u_;
  float diff_v_;
  float dt_sim_;

  // Ping-pong buffers
  // We allocate these on the heap in begin() to save space when not in use, 
  // or statically if we want to guarantee memory. Given SAMD51 192KB, 
  // 2 * 2 * 4 * 2048 = 32KB is fine.
  float *u_[2];
  float *v_[2];
  uint8_t current_buf_;

  uint16_t palette_[256];
  WeatherParams weather_;
  float phase_;
  
  uint8_t cold_green_scale_q8_;
  uint8_t allowed_indices_[16];
  uint8_t allowed_count_;
  uint8_t last_temp_warm_;
  float wind_x_;
  float wind_y_;
  
  void step();
  float laplacian(int x, int y, const float *grid);
  void seed();
  void updatePalette();
};
