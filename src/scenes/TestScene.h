#pragma once

#include <Arduino.h>

#include "Scene.h"

class TestScene : public Scene {
public:
  TestScene();
  void begin(Adafruit_Protomatter &matrix) override;
  void update(uint32_t dt_ms) override;
  void render(Adafruit_Protomatter &matrix) override;

private:
  static constexpr int32_t kFixedOne = 256;

  int32_t x_fp_;
  int32_t y_fp_;
  int32_t vx_fp_;
  int32_t vy_fp_;
  uint16_t color_;
};
