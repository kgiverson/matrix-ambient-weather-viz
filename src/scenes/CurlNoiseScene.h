#pragma once

#include "Scene.h"
#include <Adafruit_Protomatter.h>

class CurlNoiseScene : public Scene {
public:
  CurlNoiseScene();
  void begin(Adafruit_Protomatter &matrix) override;
  void update(uint32_t dt_ms) override;
  void render(Adafruit_Protomatter &matrix) override;
  void setWeather(const WeatherParams &params) override;

private:
  void updatePalette();
  float noise3D(float x, float y, float z);

  // Scene state
  float z_offset_;
  float time_speed_;
  float noise_scale_;
  uint16_t palette_[16];
  WeatherParams weather_;

  // Weather-derived targets
  float target_time_speed_;
  float target_noise_scale_;

  // Palette state (mirrors FlowField/RD patterns)
  uint8_t last_temp_warm_;
  uint8_t allowed_indices_[16];
  uint8_t allowed_count_;
  uint8_t cold_green_scale_q8_;
};
