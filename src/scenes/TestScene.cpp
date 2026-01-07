#include "scenes/TestScene.h"

#include "BoardConfig.h"

TestScene::TestScene()
    : x_fp_(0),
      y_fp_(0),
      vx_fp_(0),
      vy_fp_(0),
      color_(0) {}

void TestScene::begin(Adafruit_Protomatter &matrix) {
  x_fp_ = (kMatrixWidth / 2) * kFixedOne;
  y_fp_ = (kMatrixHeight / 2) * kFixedOne;
  vx_fp_ = 40 * kFixedOne;
  vy_fp_ = 32 * kFixedOne;
  color_ = matrix.color565(0, 200, 40);
}

void TestScene::update(uint32_t dt_ms) {
  x_fp_ += (vx_fp_ * (int32_t)dt_ms) / 1000;
  y_fp_ += (vy_fp_ * (int32_t)dt_ms) / 1000;

  const int32_t max_x = (kMatrixWidth - 1) * kFixedOne;
  const int32_t max_y = (kMatrixHeight - 1) * kFixedOne;

  if (x_fp_ < 0) {
    x_fp_ = 0;
    vx_fp_ = -vx_fp_;
  } else if (x_fp_ > max_x) {
    x_fp_ = max_x;
    vx_fp_ = -vx_fp_;
  }

  if (y_fp_ < 0) {
    y_fp_ = 0;
    vy_fp_ = -vy_fp_;
  } else if (y_fp_ > max_y) {
    y_fp_ = max_y;
    vy_fp_ = -vy_fp_;
  }
}

void TestScene::render(Adafruit_Protomatter &matrix) {
  uint16_t *buffer = matrix.getBuffer();
  const uint32_t count = (uint32_t)kMatrixWidth * kMatrixHeight;
  for (uint32_t i = 0; i < count; ++i) {
    buffer[i] = 0;
  }

  const int16_t x = (int16_t)(x_fp_ >> 8);
  const int16_t y = (int16_t)(y_fp_ >> 8);
  buffer[(y * kMatrixWidth) + x] = color_;
}
