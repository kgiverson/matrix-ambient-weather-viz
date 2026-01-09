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
      gravity_add_(0),
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
  gravity_add_ = 0;
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

  for (uint16_t i = 0; i < active_particles_; ++i) {
    Particle &p = particles_[i];
    const int16_t x = (int16_t)(p.x_fp >> kFixedShift);
    const int16_t y = (int16_t)(p.y_fp >> kFixedShift);

    const uint8_t fx = (uint8_t)((uint16_t)x * kFieldCols / kMatrixWidth);
    const uint8_t fy = (uint8_t)((uint16_t)y * kFieldRows / kMatrixHeight);
    const uint16_t field_index = (uint16_t)(fy * kFieldCols + fx);
    const Vec2 dir = direction(field_[field_index]);

    const int32_t vx_fp = (int32_t)dir.x * particle_speed_;
    const int32_t vy_fp = (int32_t)dir.y * particle_speed_ + (int32_t)gravity_add_ * 256; // Add gravity (heavier)

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

    // Respawn old particles to prevent clumping in sinkholes
    if (p.age > 240 + (i & 0x0F)) {
      p.x_fp = (int32_t)((nextRand(rng_) % kMatrixWidth) << kFixedShift);
      p.y_fp = (int32_t)((nextRand(rng_) % kMatrixHeight) << kFixedShift);
      p.age = (uint8_t)(nextRand(rng_) & 0x3F); // Start young
    }
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

  for (uint16_t i = 0; i < active_particles_; ++i) {
    const int16_t x = (int16_t)(particles_[i].x_fp >> kFixedShift);
    const int16_t y = (int16_t)(particles_[i].y_fp >> kFixedShift);
    if ((uint16_t)x < kMatrixWidth && (uint16_t)y < kMatrixHeight) {
      // Stable color based on particle index to prevent twinkling
      const uint8_t base = (uint8_t)((i % 16) + palette_offset_);
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

  // Band-based Palette Selection (Per COLOR.md)
  uint8_t allowed[16];
  uint8_t allowed_count = 0;
  
  // Cold Green Scale: Dampens green channel in Cyan/Teal to prevent "spring" look in cold
  // Default to full brightness (255)
  cold_green_scale_q8_ = 255;
  
  if (temp_f <= 20.0f) {
    // Very Cold: Deep Blue, Indigo, Violet
    const uint8_t set[] = {0, 1, 15}; 
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    cold_green_scale_q8_ = 100; // Heavy green suppression
  } 
  else if (temp_f <= 40.0f) {
    // Cold: Blues + Icy Cyan (Suppressed Green)
    const uint8_t set[] = {0, 1, 2, 3, 15};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    cold_green_scale_q8_ = 140; // Moderate green suppression for Cyan
  }
  else if (temp_f <= 50.0f) {
    // Cool / Transition: Blues + Teal
    const uint8_t set[] = {0, 1, 2, 3, 4};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    cold_green_scale_q8_ = 180; // Light green suppression
  }
  else if (temp_f <= 65.0f) {
    // Mild: Balanced Blue/Green (Cyan, Teal, Fresh Greens)
    const uint8_t set[] = {2, 3, 4, 5, 6};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    // No suppression needed
  }
  else if (temp_f <= 80.0f) {
    // Sweet Spot: Full Spectrum
    for(uint8_t i=0; i<16; ++i) allowed[allowed_count++] = i;
  }
  else if (temp_f <= 90.0f) {
    // Hot: Greens receding, warm dominance
    const uint8_t set[] = {8, 9, 10, 11, 12};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
  }
  else {
    // Very Hot: Deep Reds/Oranges only
    const uint8_t set[] = {10, 11, 12};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
  }

  allowed_count_ = (allowed_count == 0) ? 1 : allowed_count;
  for (uint8_t i = 0; i < allowed_count_; ++i) {
    allowed_indices_[i] = allowed[i];
  }

  // Visual parameters
  int wind_q = (int)(wind_mph + (wind_mph >= 0.0f ? 0.5f : -0.5f));
  // Chicago Tuning: Cap effective wind at 35mph (since 25 is "Gusty")
  wind_q = (int)clampU16(wind_q, 0, 35);

  // Warmth determines the global tint shift in updatePalette
  // Simplified: 0-255 based loosely on temp, but much less aggressive
  uint8_t temp_warm = (uint8_t)clampU16((int)(temp_f * 2.55f), 0, 255);

  if (temp_warm != last_temp_warm_) {
    updatePalette(temp_warm);
    last_temp_warm_ = temp_warm;
  }

  if (cloud != last_cloud_) {
    fade_factor_ = (uint8_t)(236 + ((uint16_t)cloud * 16) / 100);
    // Ensure at least 60% of particles are active even in clear weather
    const uint16_t min_active = (kParticleCount * 60) / 100;
    const uint16_t span = kParticleCount - min_active;
    active_particles_ =
        (uint16_t)(min_active + ((uint32_t)cloud * span) / 100);
    last_cloud_ = cloud;
  }

  if ((uint8_t)wind_q != last_wind_) {
    // Turbulence: 80ms (Calm) -> 20ms (Stormy)
    // Formula: 80 - (wind * 2)
    int interval = 80 - (wind_q * 2);
    if (interval < 20) {
      interval = 20;
    }
    field_update_interval_ms_ = (uint16_t)interval;

    // Speed: 25px/s (Calm) -> ~90px/s (Max)
    // Formula: 25 + (wind * 1.8)
    particle_speed_ = (int16_t)(25 + (wind_q * 9) / 5);
    
    last_wind_ = (uint8_t)wind_q;
  }

  if (precip != last_precip_) {
    // Map precip % (0-100) to gravity bias (0-50)
    // 0-20%: No gravity
    // 20-100%: Linear increase
    if (precip <= 20) {
      gravity_add_ = 0;
    } else {
      gravity_add_ = (int16_t)(((uint32_t)(precip - 20) * 50) / 80); 
    }
    last_precip_ = precip;
  }

  if (params.valid && temp_q != last_temp_log_q_) {
    Serial.print("WeatherMap: temp_f=");
    Serial.print(temp_f, 1);
    Serial.print(" band=");
    if (temp_f <= 20) Serial.print("VeryCold");
    else if (temp_f <= 40) Serial.print("Cold");
    else if (temp_f <= 50) Serial.print("Cool");
    else if (temp_f <= 65) Serial.print("Mild");
    else if (temp_f <= 80) Serial.print("SweetSpot");
    else if (temp_f <= 90) Serial.print("Hot");
    else Serial.print("VeryHot");
    Serial.print(" coldGreen=");
    Serial.print(cold_green_scale_q8_);
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
  // Much subtler tinting:
  // Warmth > 128 adds slight Red/Green (Warm)
  // Warmth < 128 adds slight Blue (Cool)
  const int warm_offset = (int)warmth - 128;
  const int red_bias = (warm_offset * 20) / 128;   // Reduced from 40
  const int blue_bias = (warm_offset * -20) / 128; // Reduced from 40
  const int green_bias = (warm_offset * 5) / 128;  // Reduced from 12

  for (uint8_t i = 0; i < 16; ++i) {
    const int r = (int)kBasePaletteRgb[i][0] + red_bias;
    int g = (int)kBasePaletteRgb[i][1] + green_bias;
    const int b = (int)kBasePaletteRgb[i][2] + blue_bias;
    
    // Apply Cold Green Suppression
    // This strictly reduces the Green channel for Cyan/Teal colors in cold weather
    g = (int)(((int32_t)g * (int32_t)cold_green_scale_q8_) / 255);
    
    palette_[i] = matrix.color565(clampU8(r), clampU8(g), clampU8(b));
  }
}

