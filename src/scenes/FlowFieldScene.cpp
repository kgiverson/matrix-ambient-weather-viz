#include "scenes/FlowFieldScene.h"

#include "BoardConfig.h"

namespace {
constexpr FlowFieldScene::Vec2 kDirections[16] = {
  {256, 0},
  {237, 98},
  {181, 181},
  {98, 237},
  {0, 256},
  {-98, 237},
  {-181, 181},
  {-237, 98},
  {-256, 0},
  {-237, -98},
  {-181, -181},
  {-98, -237},
  {0, -256},
  {98, -237},
  {181, -181},
  {237, -98},
};
} // namespace

FlowFieldScene::FlowFieldScene()
    : rng_(0x12345678),
      field_accum_ms_(0),
      drift_accum_ms_(0),
      field_cursor_(0),
      palette_{},
      palette_offset_(0),
      drift_bias_(0),
      first_render_(true),
      field_{},
      particles_{} {}

uint32_t FlowFieldScene::nextRand(uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

FlowFieldScene::Vec2 FlowFieldScene::direction(uint8_t idx) {
  return kDirections[idx & 0x0F];
}

void FlowFieldScene::begin(Adafruit_Protomatter &matrix) {
  rng_ ^= millis();
  field_accum_ms_ = 0;
  drift_accum_ms_ = 0;
  field_cursor_ = 0;
  palette_offset_ = (uint8_t)(nextRand(rng_) & 0x0F);
  drift_bias_ = (uint8_t)(nextRand(rng_) & 0x0F);
  first_render_ = true;

  for (uint16_t i = 0; i < kFieldSize; ++i) {
    field_[i] = (uint8_t)(nextRand(rng_) & 0x0F);
  }

  const uint8_t palette_rgb[16][3] = {
    {0, 24, 40},   {0, 56, 96},   {0, 96, 120},  {0, 140, 120},
    {0, 180, 100}, {0, 200, 40},  {40, 220, 40}, {80, 220, 60},
    {120, 200, 60}, {160, 180, 80}, {200, 140, 60}, {220, 100, 40},
    {200, 80, 80}, {160, 60, 120}, {120, 40, 140}, {80, 24, 120},
  };
  for (uint8_t i = 0; i < 16; ++i) {
    palette_[i] = matrix.color565(palette_rgb[i][0], palette_rgb[i][1],
                                  palette_rgb[i][2]);
  }

  for (uint16_t i = 0; i < kParticleCount; ++i) {
    const uint16_t x = (uint16_t)(nextRand(rng_) % kMatrixWidth);
    const uint16_t y = (uint16_t)(nextRand(rng_) % kMatrixHeight);
    particles_[i].x_fp = (int32_t)(x << kFixedShift);
    particles_[i].y_fp = (int32_t)(y << kFixedShift);
    particles_[i].age = (uint8_t)(nextRand(rng_) & 0xFF);
    particles_[i].pad = 0;
  }
}

void FlowFieldScene::update(uint32_t dt_ms) {
  field_accum_ms_ += dt_ms;
  while (field_accum_ms_ >= kFieldUpdateIntervalMs) {
    field_accum_ms_ -= kFieldUpdateIntervalMs;
    const uint16_t idx = (uint16_t)(field_cursor_ % kFieldSize);
    const uint8_t dir = field_[idx];
    const uint8_t delta = (uint8_t)(nextRand(rng_) & 0x01);
    const uint8_t bias = (uint8_t)((drift_bias_ & 0x03) != 0);
    field_[idx] = (uint8_t)((dir + (delta ? 1 : 15) + bias) & 0x0F);
    field_cursor_++;
  }

  drift_accum_ms_ += dt_ms;
  while (drift_accum_ms_ >= kDriftIntervalMs) {
    drift_accum_ms_ -= kDriftIntervalMs;
    if ((nextRand(rng_) & 0x1F) == 0) {
      palette_offset_ = (uint8_t)((palette_offset_ + 1) & 0x0F);
    }
    if ((nextRand(rng_) & 0x3F) == 0) {
      drift_bias_ = (uint8_t)((drift_bias_ + 1) & 0x0F);
    }
  }

  for (uint16_t i = 0; i < kParticleCount; ++i) {
    Particle &p = particles_[i];
    const int16_t x = (int16_t)(p.x_fp >> kFixedShift);
    const int16_t y = (int16_t)(p.y_fp >> kFixedShift);

    const uint8_t fx = (uint8_t)((uint16_t)x * kFieldCols / kMatrixWidth);
    const uint8_t fy = (uint8_t)((uint16_t)y * kFieldRows / kMatrixHeight);
    const uint16_t field_index = (uint16_t)(fy * kFieldCols + fx);
    const Vec2 dir = direction(field_[field_index]);

    const int32_t vx_fp = (int32_t)dir.x * kParticleSpeed;
    const int32_t vy_fp = (int32_t)dir.y * kParticleSpeed;

    p.x_fp += (vx_fp * (int32_t)dt_ms) / 1000;
    p.y_fp += (vy_fp * (int32_t)dt_ms) / 1000;

    const int32_t max_x = (int32_t)kMatrixWidth * kFixedOne;
    const int32_t max_y = (int32_t)kMatrixHeight * kFixedOne;

    if (p.x_fp < 0) {
      p.x_fp += max_x;
    } else if (p.x_fp >= max_x) {
      p.x_fp -= max_x;
    }

    if (p.y_fp < 0) {
      p.y_fp += max_y;
    } else if (p.y_fp >= max_y) {
      p.y_fp -= max_y;
    }

    p.age++;
  }
}

void FlowFieldScene::render(Adafruit_Protomatter &matrix) {
  if (first_render_) {
    Serial.print("[");
    Serial.print(millis());
    Serial.println("] FlowField: first render");
    first_render_ = false;
  }

  uint16_t *buffer = matrix.getBuffer();
  const uint32_t count = (uint32_t)kMatrixWidth * kMatrixHeight;
  for (uint32_t i = 0; i < count; ++i) {
    const uint16_t c = buffer[i];
    if (c == 0) {
      continue;
    }
    uint8_t r = (uint8_t)((c >> 11) & 0x1F);
    uint8_t g = (uint8_t)((c >> 5) & 0x3F);
    uint8_t b = (uint8_t)(c & 0x1F);
    r = (uint8_t)((r * kFadeFactor) >> 8);
    g = (uint8_t)((g * kFadeFactor) >> 8);
    b = (uint8_t)((b * kFadeFactor) >> 8);
    buffer[i] = (uint16_t)((r << 11) | (g << 5) | b);
  }

  for (uint16_t i = 0; i < kParticleCount; ++i) {
    const int16_t x = (int16_t)(particles_[i].x_fp >> kFixedShift);
    const int16_t y = (int16_t)(particles_[i].y_fp >> kFixedShift);
    if ((uint16_t)x < kMatrixWidth && (uint16_t)y < kMatrixHeight) {
      const uint8_t idx =
          (uint8_t)(((particles_[i].age >> 4) ^ (x + y)) + palette_offset_) &
          0x0F;
      buffer[(y * kMatrixWidth) + x] = palette_[idx];
    }
  }
}
