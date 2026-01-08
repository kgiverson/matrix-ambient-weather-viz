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

float smoothstep(float t) {
  if (t < 0.0f) {
    t = 0.0f;
  } else if (t > 1.0f) {
    t = 1.0f;
  }
  return t * t * (3.0f - 2.0f * t);
}

float smoothstep(float edge0, float edge1, float x) {
  if (edge1 <= edge0) {
    return (x >= edge1) ? 1.0f : 0.0f;
  }
  return smoothstep((x - edge0) / (edge1 - edge0));
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
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
      green_allow_q8_(255),
      red_takeover_q8_(0),
      cold_green_scale_q8_(255),
      allowed_indices_{},
      allowed_count_(16),
      last_temp_warm_(0xFF),
      last_cloud_(0xFF),
      last_wind_(0xFF),
      last_precip_(0xFF),
      last_temp_log_q_(-32768),
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

  memset(sim_buffer_, 0, sizeof(sim_buffer_));

  weather_.valid = false;
  field_update_interval_ms_ = kFieldUpdateIntervalMs;
  fade_factor_ = kFadeFactor;
  active_particles_ = kParticleCount;
  particle_speed_ = kParticleSpeed;
  precip_accum_ms_ = 0;
  precip_burst_interval_ms_ = 0;
  precip_burst_pending_ = false;
  green_allow_q8_ = 255;
  red_takeover_q8_ = 0;
  cold_green_scale_q8_ = 255;
  allowed_count_ = 16;
  for (uint8_t i = 0; i < 16; ++i) {
    allowed_indices_[i] = i;
  }
  last_temp_warm_ = 0xFF;
  last_cloud_ = 0xFF;
  last_wind_ = 0xFF;
  last_precip_ = 0xFF;
  last_temp_log_q_ = -32768;
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

  uint16_t *buffer = sim_buffer_;
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
      const uint8_t base =
          (uint8_t)(((particles_[i].age >> 4) ^ (x + y)) + palette_offset_);
      const uint8_t count = (allowed_count_ == 0) ? 1 : allowed_count_;
      const uint8_t mapped = allowed_indices_[base % count];
      const uint8_t idx = mapped & 0x0F;
      buffer[(y * kMatrixWidth) + x] = palette_[idx];
    }
  }

  uint16_t *matrix_buffer = matrix.getBuffer();
  memcpy(matrix_buffer, sim_buffer_, sizeof(sim_buffer_));
}

void FlowFieldScene::setWeather(const WeatherParams &params) {
  weather_ = params;
  const float temp_f = params.valid ? params.temp_f : 70.0f;
  const float wind_mph = params.valid ? params.wind_speed_mph : 6.0f;
  const uint8_t cloud = params.valid ? params.cloud_cover_pct : 35;
  const uint8_t precip = params.valid ? params.precip_prob_pct : 0;

  int temp_q = (int)(temp_f + (temp_f >= 0.0f ? 0.5f : -0.5f));
  temp_q = (int)clampU16(temp_q, 0, 100);

  float warmth = 0.0f;
  if (temp_f <= 20.0f) {
    const float t = smoothstep((temp_f - 0.0f) / 20.0f);
    warmth = lerp(0.00f, 0.12f, t);
  } else if (temp_f <= 40.0f) {
    const float t = smoothstep((temp_f - 20.0f) / 20.0f);
    warmth = lerp(0.12f, 0.32f, t);
  } else if (temp_f <= 65.0f) {
    const float t = smoothstep((temp_f - 40.0f) / 25.0f);
    warmth = lerp(0.32f, 0.72f, t);
  } else if (temp_f <= 85.0f) {
    const float t = smoothstep((temp_f - 65.0f) / 20.0f);
    warmth = lerp(0.72f, 0.92f, t);
  } else {
    const float t = smoothstep((temp_f - 85.0f) / 15.0f);
    warmth = lerp(0.92f, 1.00f, t);
  }

  if (warmth < 0.0f) {
    warmth = 0.0f;
  } else if (warmth > 1.0f) {
    warmth = 1.0f;
  }
  const uint8_t temp_warm = (uint8_t)(warmth * 255.0f + 0.5f);
  const float green_allow = smoothstep(50.0f, 65.0f, temp_f);
  const float rainbow =
      smoothstep(65.0f, 70.0f, temp_f) *
      (1.0f - smoothstep(80.0f, 85.0f, temp_f));
  const float red_takeover = smoothstep(80.0f, 92.0f, temp_f);
  const float cold_green_scale = lerp(0.55f, 1.0f, rainbow);
  green_allow_q8_ = (uint8_t)(green_allow * 255.0f + 0.5f);
  red_takeover_q8_ = (uint8_t)(red_takeover * 255.0f + 0.5f);
  cold_green_scale_q8_ = (uint8_t)(cold_green_scale * 255.0f + 0.5f);

  uint8_t allowed[16];
  uint8_t allowed_count = 0;
  const uint8_t cold_safe[] = {0, 1, 2, 15, 14, 13};
  for (uint8_t i = 0; i < sizeof(cold_safe); ++i) {
    allowed[allowed_count++] = cold_safe[i];
  }
  if (rainbow >= 0.2f) {
    allowed[allowed_count++] = 3;
    allowed[allowed_count++] = 4;
  }
  if (green_allow > 0.2f) {
    const uint8_t greens[] = {5, 6, 7, 8, 9};
    for (uint8_t i = 0; i < sizeof(greens); ++i) {
      allowed[allowed_count++] = greens[i];
    }
  }
  if (red_takeover > 0.2f) {
    const uint8_t hot[] = {10, 11, 12};
    for (uint8_t i = 0; i < sizeof(hot); ++i) {
      allowed[allowed_count++] = hot[i];
    }
  }
  allowed_count_ = (allowed_count == 0) ? 1 : allowed_count;
  for (uint8_t i = 0; i < allowed_count_; ++i) {
    allowed_indices_[i] = allowed[i];
  }

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

  if (params.valid && temp_q != last_temp_log_q_) {
    Serial.print("WeatherMap: temp_f=");
    Serial.print(temp_f, 1);
    Serial.print(" warmth=");
    Serial.print(warmth, 3);
    Serial.print(" rainbow=");
    Serial.print(rainbow, 3);
    Serial.print(" greenAllow=");
    Serial.print(green_allow, 3);
    Serial.print(" redTakeover=");
    Serial.print(red_takeover, 3);
    Serial.print(" coldGreen=");
    Serial.print(cold_green_scale, 3);
    Serial.print(" allowedCount=");
    Serial.print(allowed_count_);
    Serial.print(" idxList=");
    for (uint8_t i = 0; i < allowed_count_; ++i) {
      if (i > 0) {
        Serial.print(",");
      }
      Serial.print(allowed_indices_[i]);
    }
    Serial.println();
    last_temp_log_q_ = (int16_t)temp_q;
  }
}

void FlowFieldScene::updatePalette(uint8_t warmth) {
  const int warm_offset = (int)warmth - 128;
  const int red_bias = (warm_offset * 40) / 128;
  const int blue_bias = (warm_offset * -40) / 128;
  const int green_bias = (warm_offset * 12) / 128;

  for (uint8_t i = 0; i < 16; ++i) {
    const int r = (int)kBasePaletteRgb[i][0] + red_bias;
    int g = (int)kBasePaletteRgb[i][1] + green_bias;
    const int b = (int)kBasePaletteRgb[i][2] + blue_bias;
    g = (int)(((int32_t)g * (int32_t)cold_green_scale_q8_) / 255);
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
