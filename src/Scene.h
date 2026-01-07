#pragma once

#include <Arduino.h>
#include <Adafruit_Protomatter.h>

class Scene {
public:
  virtual ~Scene() = default;
  virtual void begin(Adafruit_Protomatter &matrix) {
    (void)matrix;
  }
  virtual void update(uint32_t dt_ms) = 0;
  virtual void render(Adafruit_Protomatter &matrix) = 0;
};
