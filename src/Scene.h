#pragma once

#include <Arduino.h>
#include <Adafruit_Protomatter.h>

struct WeatherParams {
  float temp_f;
  float wind_speed_mph;
  uint8_t cloud_cover_pct;
  uint8_t precip_prob_pct;
  bool valid;
};

class Scene {
public:
  virtual ~Scene() = default;
  virtual void begin(Adafruit_Protomatter &matrix) {
    (void)matrix;
  }
  virtual void update(uint32_t dt_ms) = 0;
  virtual void render(Adafruit_Protomatter &matrix) = 0;
  virtual void setWeather(const WeatherParams &params) {
    (void)params;
  }
};