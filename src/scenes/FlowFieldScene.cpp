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

constexpr uint8_t kBasePaletteRgb[16][3] = {
  {0, 24, 40},   {0, 56, 96},   {0, 96, 120},  {0, 140, 120},
  {0, 180, 100}, {0, 200, 40},  {40, 220, 40}, {80, 220, 60},
  {120, 200, 60}, {160, 180, 80}, {200, 140, 60}, {220, 100, 40},
  {200, 80, 80}, {160, 60, 120}, {120, 40, 140}, {80, 24, 120},
};

uint8_t clampU8(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return (uint8_t)value;
}

uint16_t clampU16(int value, int min_value, int max_value) {
  if (value < min_value) {
    return (uint16_t)min_value;
  }
  if (value > max_value) {
    return (uint16_t)max_value;
  }
  return (uint16_t)value;
}
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
      field_update_interval_ms_(kFieldUpdateIntervalMs),
      fade_factor_(kFadeFactor),
      active_particles_(kParticleCount),
      particle_speed_(kParticleSpeed),
      precip_accum_ms_(0),
      precip_burst_interval_ms_(0),
      precip_burst_pending_(false),
      last_temp_warm_(0xFF),
      last_cloud_(0xFF),
      last_wind_(0xFF),
      last_precip_(0xFF),
      field_{},
      particles_{},
      weather_{} {}

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

  updatePalette(128);

  for (uint16_t i = 0; i < kParticleCount; ++i) {
    const uint16_t x = (uint16_t)(nextRand(rng_) % kMatrixWidth);
    const uint16_t y = (uint16_t)(nextRand(rng_) % kMatrixHeight);
    particles_[i].x_fp = (int32_t)(x << kFixedShift);
    particles_[i].y_fp = (int32_t)(y << kFixedShift);
    particles_[i].age = (uint8_t)(nextRand(rng_) & 0xFF);
    particles_[i].pad = 0;
  }

  weather_.valid = false;
  field_update_interval_ms_ = kFieldUpdateIntervalMs;
  fade_factor_ = kFadeFactor;
  active_particles_ = kParticleCount;
  particle_speed_ = kParticleSpeed;
  precip_accum_ms_ = 0;
  precip_burst_interval_ms_ = 0;
  precip_burst_pending_ = false;
  last_temp_warm_ = 0xFF;
  last_cloud_ = 0xFF;
  last_wind_ = 0xFF;
  last_precip_ = 0xFF;
}

void FlowFieldScene::update(uint32_t dt_ms) {
  field_accum_ms_ += dt_ms;
  while (field_accum_ms_ >= field_update_interval_ms_) {
    field_accum_ms_ -= field_update_interval_ms_;
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

  if (precip_burst_interval_ms_ > 0) {
    precip_accum_ms_ += dt_ms;
    if (precip_accum_ms_ >= precip_burst_interval_ms_) {
      precip_accum_ms_ -= precip_burst_interval_ms_;
      applyPrecipBurst();
    }
  }

  for (uint16_t i = 0; i < active_particles_; ++i) {
    Particle &p = particles_[i];
    const int16_t x = (int16_t)(p.x_fp >> kFixedShift);
    const int16_t y = (int16_t)(p.y_fp >> kFixedShift);

    const uint8_t fx = (uint8_t)((uint16_t)x * kFieldCols / kMatrixWidth);
    const uint8_t fy = (uint8_t)((uint16_t)y * kFieldRows / kMatrixHeight);
    const uint16_t field_index = (uint16_t)(fy * kFieldCols + fx);
    const Vec2 dir = direction(field_[field_index]);

    const int32_t vx_fp = (int32_t)dir.x * particle_speed_;
    const int32_t vy_fp = (int32_t)dir.y * particle_speed_;

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
  if (precip_burst_pending_) {
    for (uint32_t i = 0; i < count; ++i) {
      buffer[i] = 0;
    }
    precip_burst_pending_ = false;
  } else {
    for (uint32_t i = 0; i < count; ++i) {
      const uint16_t c = buffer[i];
      if (c == 0) {
        continue;
      }
      uint8_t r = (uint8_t)((c >> 11) & 0x1F);
      uint8_t g = (uint8_t)((c >> 5) & 0x3F);
      uint8_t b = (uint8_t)(c & 0x1F);
      r = (uint8_t)((r * fade_factor_) >> 8);
      g = (uint8_t)((g * fade_factor_) >> 8);
      b = (uint8_t)((b * fade_factor_) >> 8);
      buffer[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
  }

  for (uint16_t i = 0; i < active_particles_; ++i) {
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

void FlowFieldScene::setWeather(const WeatherParams &params) {
  weather_ = params;
  const float temp_f = params.valid ? params.temp_f : 70.0f;
  const float wind_mph = params.valid ? params.wind_speed_mph : 6.0f;
  const uint8_t cloud = params.valid ? params.cloud_cover_pct : 35;
  const uint8_t precip = params.valid ? params.precip_prob_pct : 0;

  int temp_q = (int)(temp_f + (temp_f >= 0.0f ? 0.5f : -0.5f));
  temp_q = (int)clampU16(temp_q, 0, 100);
  const int temp_clamped = (temp_q < 0) ? 0 : (temp_q > 90 ? 90 : temp_q);
  const uint8_t temp_warm =
      (uint8_t)((temp_clamped * 255) / 90);

  int wind_q = (int)(wind_mph + (wind_mph >= 0.0f ? 0.5f : -0.5f));
  wind_q = (int)clampU16(wind_q, 0, 40);

  if (temp_warm != last_temp_warm_) {
    updatePalette(temp_warm);
    last_temp_warm_ = temp_warm;
  }

  if (cloud != last_cloud_) {
    fade_factor_ = (uint8_t)(236 + ((uint16_t)cloud * 16) / 100);
    const uint16_t span = kParticleCount - kMinParticles;
    active_particles_ =
        (uint16_t)(kMinParticles + ((uint32_t)cloud * span) / 100);
    last_cloud_ = cloud;
  }

  if ((uint8_t)wind_q != last_wind_) {
    field_update_interval_ms_ =
        (uint16_t)(60 - ((uint32_t)wind_q * 40) / 40);
    if (field_update_interval_ms_ < 16) {
      field_update_interval_ms_ = 16;
    }
    particle_speed_ = (int16_t)(30 + ((uint32_t)wind_q * 40) / 40);
    last_wind_ = (uint8_t)wind_q;
  }

  if (precip != last_precip_) {
    if (precip < 15) {
      precip_burst_interval_ms_ = 0;
    } else {
      const uint16_t interval =
          (uint16_t)(30000 - ((uint32_t)precip * 200));
      precip_burst_interval_ms_ =
          clampU16(interval, 8000, 30000);
    }
    precip_accum_ms_ = 0;
    last_precip_ = precip;
  }
}

void FlowFieldScene::updatePalette(uint8_t warmth) {
  const int warm_offset = (int)warmth - 128;
  const int red_bias = (warm_offset * 40) / 128;
  const int blue_bias = (warm_offset * -40) / 128;
  const int green_bias = (warm_offset * 12) / 128;

  for (uint8_t i = 0; i < 16; ++i) {
    const int r = (int)kBasePaletteRgb[i][0] + red_bias;
    const int g = (int)kBasePaletteRgb[i][1] + green_bias;
    const int b = (int)kBasePaletteRgb[i][2] + blue_bias;
    palette_[i] = matrix.color565(clampU8(r), clampU8(g), clampU8(b));
  }
}

void FlowFieldScene::applyPrecipBurst() {
  for (uint16_t i = 0; i < kFieldSize; ++i) {
    field_[i] = (uint8_t)(nextRand(rng_) & 0x0F);
  }
  field_cursor_ = 0;
  drift_bias_ = (uint8_t)(nextRand(rng_) & 0x0F);
  palette_offset_ = (uint8_t)(nextRand(rng_) & 0x0F);
  for (uint16_t i = 0; i < kParticleCount; ++i) {
    const uint16_t x = (uint16_t)(nextRand(rng_) % kMatrixWidth);
    const uint16_t y = (uint16_t)(nextRand(rng_) % kMatrixHeight);
    particles_[i].x_fp = (int32_t)(x << kFixedShift);
    particles_[i].y_fp = (int32_t)(y << kFixedShift);
    particles_[i].age = (uint8_t)(nextRand(rng_) & 0xFF);
    particles_[i].pad = 0;
  }
  precip_burst_pending_ = true;
}
