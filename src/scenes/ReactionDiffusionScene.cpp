#include "scenes/ReactionDiffusionScene.h"
#include "BoardConfig.h"
#include "PaletteUtils.h"

namespace {
// Color palette for the RD scene (Heatmap style: Blue -> Cyan -> Green -> Yellow -> Red)
// We generate a 256-entry lookup table based on the 'v' concentration.
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
} // namespace

ReactionDiffusionScene::ReactionDiffusionScene()
    : feed_(0.037f), kill_(0.060f), diff_u_(1.0f), diff_v_(0.5f), dt_sim_(0.5f),
      current_buf_(0), weather_{}, phase_(0.0f),
      cold_green_scale_q8_(255), allowed_count_(16), last_temp_warm_(0xFF) {
  u_[0] = nullptr;
  u_[1] = nullptr;
  v_[0] = nullptr;
  v_[1] = nullptr;
  for (uint8_t i = 0; i < 16; ++i) {
    allowed_indices_[i] = i;
  }
}

ReactionDiffusionScene::~ReactionDiffusionScene() {
  if (u_[0]) delete[] u_[0];
  if (u_[1]) delete[] u_[1];
  if (v_[0]) delete[] v_[0];
  if (v_[1]) delete[] v_[1];
}

void ReactionDiffusionScene::begin(Adafruit_Protomatter &matrix) {
  (void)matrix;
  Serial.println("RD: begin");
  
  // Allocate buffers if not already done
  if (!u_[0]) {
    u_[0] = new float[kGridSize];
    u_[1] = new float[kGridSize];
    v_[0] = new float[kGridSize];
    v_[1] = new float[kGridSize];
    Serial.println("RD: allocated buffers");
  }

  seed();
  
  // Initial palette setup if weather hasn't arrived
  if (!weather_.valid) {
    WeatherParams default_params{};
    default_params.temp_f = 68.0f;
    default_params.valid = true;
    setWeather(default_params);
  }
  updatePalette();
}

void ReactionDiffusionScene::seed() {
  Serial.println("RD: seeding");
  // Initialize: u=1, v=0 everywhere
  for (int i = 0; i < kGridSize; ++i) {
    u_[0][i] = 1.0f;
    v_[0][i] = 0.0f;
    u_[1][i] = 1.0f;
    v_[1][i] = 0.0f;
  }

  // Seed with random blocks of v=1, u=0.5
  for (int i = 0; i < 12; ++i) {
    int cx = random(kWidth);
    int cy = random(kHeight);
    int r = random(1, 4);
    
    for (int y = cy - r; y <= cy + r; ++y) {
      for (int x = cx - r; x <= cx + r; ++x) {
        if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
          int idx = y * kWidth + x;
          u_[0][idx] = 0.5f;
          v_[0][idx] = 0.25f + (float)random(100) / 200.0f;
        }
      }
    }
  }
}

// Simple 3x3 Laplacian kernel
float ReactionDiffusionScene::laplacian(int x, int y, const float *grid) {
  float sum = 0.0f;
  
  // Center
  int idx = y * kWidth + x;
  sum += grid[idx] * -1.0f;

  // Orthogonal neighbors (0.2)
  int xm1 = (x - 1 + kWidth) % kWidth;
  int xp1 = (x + 1) % kWidth;
  int ym1 = (y - 1 + kHeight) % kHeight;
  int yp1 = (y + 1) % kHeight;

  sum += grid[y * kWidth + xm1] * 0.2f;
  sum += grid[y * kWidth + xp1] * 0.2f;
  sum += grid[ym1 * kWidth + x] * 0.2f;
  sum += grid[yp1 * kWidth + x] * 0.2f;

  // Diagonal neighbors (0.05)
  sum += grid[ym1 * kWidth + xm1] * 0.05f;
  sum += grid[ym1 * kWidth + xp1] * 0.05f;
  sum += grid[yp1 * kWidth + xm1] * 0.05f;
  sum += grid[yp1 * kWidth + xp1] * 0.05f;

  return sum;
}

void ReactionDiffusionScene::step() {
  const float *u = u_[current_buf_];
  const float *v = v_[current_buf_];
  
  int next_buf = 1 - current_buf_;
  float *next_u = u_[next_buf];
  float *next_v = v_[next_buf];

  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      int i = y * kWidth + x;
      
      float u_val = u[i];
      float v_val = v[i];
      
      float uvv = u_val * v_val * v_val;
      float lap_u = laplacian(x, y, u);
      float lap_v = laplacian(x, y, v);

      float du = (diff_u_ * lap_u) - uvv + (feed_ * (1.0f - u_val));
      float dv = (diff_v_ * lap_v) + uvv - ((feed_ + kill_) * v_val);

      // Add a tiny bit of noise to prevent "perfect" static equilibrium
      if ((i & 0x7F) == 0) {
        dv += ((float)random(100) - 50.0f) * 0.0001f;
      }

      next_u[i] = u_val + (du * dt_sim_);
      next_v[i] = v_val + (dv * dt_sim_);

      // Clamp
      if (next_u[i] < 0.0f) next_u[i] = 0.0f;
      else if (next_u[i] > 1.0f) next_u[i] = 1.0f;
      
      if (next_v[i] < 0.0f) next_v[i] = 0.0f;
      else if (next_v[i] > 1.0f) next_v[i] = 1.0f;
    }
  }

  current_buf_ = next_buf;
}

void ReactionDiffusionScene::update(uint32_t dt_ms) {
  phase_ += (float)dt_ms * 0.0005f; // Faster drift
  
  // Stronger modulation for more visible movement
  float drift_f = sinf(phase_) * 0.004f;
  float drift_k = cosf(phase_ * 0.7f) * 0.002f;
  
  float active_feed = feed_ + drift_f;
  float active_kill = kill_ + drift_k;

  // Rain: 2x2 drops for better visibility
  if (weather_.valid && weather_.precip_prob_pct > 10) {
    if (random(100) < (weather_.precip_prob_pct / 8)) {
      int rx = random(kWidth - 1);
      int ry = random(kHeight - 1);
      for (int dy=0; dy<2; ++dy) {
        for (int dx=0; dx<2; ++dx) {
          v_[current_buf_][(ry+dy) * kWidth + (rx+dx)] = 0.9f;
        }
      }
    }
  }

  for (int i = 0; i < 20; ++i) {
    float old_f = feed_;
    float old_k = kill_;
    feed_ = active_feed;
    kill_ = active_kill;
    step();
    feed_ = old_f;
    kill_ = old_k;
  }

  static uint32_t last_check = 0;
  if (millis() - last_check > 5000) {
    last_check = millis();
    float total_v = 0;
    float max_v = 0;
    for(int i=0; i<kGridSize; ++i) {
      float val = v_[current_buf_][i];
      total_v += val;
      if (val > max_v) max_v = val;
    }
    float avg_v = total_v / kGridSize;

    Serial.print("RD: avg_v=");
    Serial.print(avg_v, 4);
    Serial.print(" max_v=");
    Serial.println(max_v, 4);
    
    if (total_v < (kGridSize * 0.005f) || max_v < 0.01f) {
      Serial.println("RD: field died, reseeding");
      seed();
    }
  }
}

void ReactionDiffusionScene::render(Adafruit_Protomatter &matrix) {
  const float *v = v_[current_buf_];
  
  uint16_t *buffer = matrix.getBuffer();
  
  for (int i = 0; i < kGridSize; ++i) {
    // Map v (0.0 - 1.0) to palette index (0 - 255)
    // Adjusted multiplier to prevent over-saturation and show more movement detail
    float val = v[i];
    int idx = (int)(val * 600.0f); 
    if (idx < 0) idx = 0;
    if (idx > 255) idx = 255;
    
    buffer[i] = palette_[idx];
  }
}

void ReactionDiffusionScene::setWeather(const WeatherParams &params) {
  weather_ = params;
  
  if (params.valid) {
    // Range F: 0.03 - 0.07 (Spots to Stripes)
    float t = (params.temp_f - 20.0f) / 80.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    feed_ = 0.030f + (t * 0.035f); 
    kill_ = 0.060f + (t * 0.005f); 
    
    // Standard diffusion (1.0 / 0.5) with slight weather modulation
    float w = params.wind_speed_mph / 40.0f;
    if (w > 1.0f) w = 1.0f;
    
    diff_u_ = 1.0f + (w * 0.2f);
    diff_v_ = 0.5f + (w * 0.1f);
    dt_sim_ = 0.5f + (w * 0.2f); // Faster time with wind

    // Temperature-based banding logic (Shared with FlowFieldScene)
    uint8_t allowed[16];
    uint8_t allowed_count = 0;
    cold_green_scale_q8_ = 255;
    
    if (params.temp_f <= 20.0f) {
      const uint8_t set[] = {0, 1, 15}; 
      for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
      cold_green_scale_q8_ = 100;
    } 
    else if (params.temp_f <= 40.0f) {
      const uint8_t set[] = {0, 1, 2, 3, 15};
      for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
      cold_green_scale_q8_ = 140;
    }
    else if (params.temp_f <= 50.0f) {
      const uint8_t set[] = {0, 1, 2, 3, 4};
      for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
      cold_green_scale_q8_ = 180;
    }
    else if (params.temp_f <= 65.0f) {
      const uint8_t set[] = {2, 3, 4, 5, 6};
      for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    }
    else if (params.temp_f <= 80.0f) {
      for(uint8_t i=0; i<16; ++i) allowed[allowed_count++] = i;
    }
    else if (params.temp_f <= 90.0f) {
      const uint8_t set[] = {8, 9, 10, 11, 12};
      for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    }
    else {
      const uint8_t set[] = {10, 11, 12};
      for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    }

    allowed_count_ = (allowed_count == 0) ? 1 : allowed_count;
    for (uint8_t i = 0; i < allowed_count_; ++i) {
      allowed_indices_[i] = allowed[i];
    }

    uint8_t temp_warm = (uint8_t)(t * 255.0f);
    if (temp_warm != last_temp_warm_) {
      updatePalette();
      last_temp_warm_ = temp_warm;
    }
  }
}

void ReactionDiffusionScene::updatePalette() {
  // Use last_temp_warm_ as the warmth bias
  const int warm_offset = (int)last_temp_warm_ - 128;
  const int red_bias = (warm_offset * 20) / 128;
  const int blue_bias = (warm_offset * -20) / 128;
  const int green_bias = (warm_offset * 5) / 128;

  // 1. Prepare the filtered RGB colors for the ramp
  uint8_t ramp_colors[16][3];
  for (uint8_t i = 0; i < allowed_count_; ++i) {
    uint8_t base_idx = allowed_indices_[i];
    int r = (int)PaletteUtils::kBasePaletteRgb[base_idx][0] + red_bias;
    int g = (int)PaletteUtils::kBasePaletteRgb[base_idx][1] + green_bias;
    int b = (int)PaletteUtils::kBasePaletteRgb[base_idx][2] + blue_bias;
    g = (int)(((int32_t)g * (int32_t)cold_green_scale_q8_) / 255);
    
    ramp_colors[i][0] = PaletteUtils::clampU8(r);
    ramp_colors[i][1] = PaletteUtils::clampU8(g);
    ramp_colors[i][2] = PaletteUtils::clampU8(b);
  }

  // 2. Create 256-color gradient by interpolating between the filtered colors
  // We'll go from Black (0,0,0) -> ramp_colors[0] -> ... -> ramp_colors[allowed_count-1]
  // Black occupies the first chunk to keep the "background" dark.
  
  for (int i = 0; i < 256; ++i) {
    if (i < 32) {
      // Fade from Black to first color
      float t = (float)i / 32.0f;
      uint8_t black[3] = {0, 0, 0};
      uint8_t out[3];
      PaletteUtils::lerpRgb(black, ramp_colors[0], t, out);
      palette_[i] = PaletteUtils::color565(out[0], out[1], out[2]);
    } else {
      // Interpolate between the allowed ramp colors
      float norm = (float)(i - 32) / (255.0f - 32.0f); // 0.0 to 1.0
      float segment_f = norm * (allowed_count_ - 1);
      int idx1 = (int)segment_f;
      int idx2 = (idx1 + 1 < allowed_count_) ? idx1 + 1 : idx1;
      float t = segment_f - idx1;
      
      uint8_t out[3];
      PaletteUtils::lerpRgb(ramp_colors[idx1], ramp_colors[idx2], t, out);
      palette_[i] = PaletteUtils::color565(out[0], out[1], out[2]);
    }
  }
}