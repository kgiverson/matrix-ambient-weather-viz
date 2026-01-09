#include "Engine.h"

Engine::Engine(Adafruit_Protomatter &matrix, uint32_t frame_interval_ms)
    : matrix_(matrix),
      scene_(nullptr),
      frame_interval_ms_(frame_interval_ms),
      last_frame_ms_(0) {}

void Engine::setScene(Scene *scene) {
  scene_ = scene;
}

void Engine::begin() {
  if (scene_) {
    scene_->begin(matrix_);
  }
}

void Engine::tick(uint32_t now_ms, float dimmer) {
  if (!scene_) {
    return;
  }

  if ((uint32_t)(now_ms - last_frame_ms_) < frame_interval_ms_) {
    return;
  }

  uint32_t dt_ms = (last_frame_ms_ == 0) ? frame_interval_ms_
                                         : (now_ms - last_frame_ms_);
  if (dt_ms > 100) {
    dt_ms = 100;
  }
  last_frame_ms_ = now_ms;

  scene_->update(dt_ms);
  scene_->render(matrix_);

  // Apply Global Dimming (e.g. for weather fetch fade-out)
  if (dimmer < 0.99f) {
    if (dimmer <= 0.01f) {
      matrix_.fillScreen(0);
    } else {
      // Fixed-point scale (0-256)
      const uint16_t scale = (uint16_t)(dimmer * 256.0f);
      const int16_t w = matrix_.width();
      const int16_t h = matrix_.height();
      
      for(int16_t y=0; y<h; ++y) {
        for(int16_t x=0; x<w; ++x) {
          uint16_t color = matrix_.getPixel(x, y);
          // Unpack 565
          uint32_t r = (color >> 11) & 0x1F;
          uint32_t g = (color >> 5) & 0x3F;
          uint32_t b = color & 0x1F;
          
          // Scale
          r = (r * scale) >> 8;
          g = (g * scale) >> 8;
          b = (b * scale) >> 8;
          
          // Repack
          matrix_.drawPixel(x, y, (uint16_t)((r << 11) | (g << 5) | b));
        }
      }
    }
  }

  matrix_.show();
}
