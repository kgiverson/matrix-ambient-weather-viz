#pragma once

#include <Arduino.h>
#include <Adafruit_Protomatter.h>

#include "Scene.h"

class Engine {
public:
  Engine(Adafruit_Protomatter &matrix, uint32_t frame_interval_ms);

  void setScene(Scene *scene);
  void begin();
  void tick(uint32_t now_ms, float dimmer = 1.0f);

private:
  Adafruit_Protomatter &matrix_;
  Scene *scene_;
  uint32_t frame_interval_ms_;
  uint32_t last_frame_ms_;
};
