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

void Engine::tick(uint32_t now_ms) {
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
  matrix_.show();
}
