#include "scenes/CurlNoiseScene.h"
#include "PaletteUtils.h"
#include "BoardConfig.h"
#include <math.h>

namespace {
// Compact Simplex Noise Implementation (referenced from common lightweight C implementations)
// This is used for 3D noise (x, y, time).
// Use uint8_t to avoid narrowing conversion errors
const uint8_t p[] = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125,
    136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41,
    55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188,
    159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227,
    47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22,
    39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162,
    241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180};

float lerp(float t, float a, float b) {
  return a + t * (b - a);
}

float grad(int hash, float x, float y, float z) {
  int h = hash & 15;
  float u = h < 8 ? x : y;
  float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float noise(float x, float y, float z) {
  int X = (int)floor(x) & 255;
  int Y = (int)floor(y) & 255;
  int Z = (int)floor(z) & 255;

  x -= floor(x);
  y -= floor(y);
  z -= floor(z);

  float u = x * x * x * (x * (x * 6 - 15) + 10);
  float v = y * y * y * (y * (y * 6 - 15) + 10);
  float w = z * z * z * (z * (z * 6 - 15) + 10);

  int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
  int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

  return 0.5f * (float)lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
                                   lerp(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))),
                           lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)),
                                lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
}
} // namespace

CurlNoiseScene::CurlNoiseScene()
    : z_offset_(0.0f), time_speed_(0.2f), noise_scale_(0.08f),
      target_time_speed_(0.2f), target_noise_scale_(0.08f),
      last_temp_warm_(0xFF), allowed_count_(16), cold_green_scale_q8_(255) {
  for (uint8_t i = 0; i < 16; ++i) {
    allowed_indices_[i] = i;
  }
}

void CurlNoiseScene::begin(Adafruit_Protomatter &matrix) {
  (void)matrix;
  Serial.println("CurlNoise: begin");
  z_offset_ = (float)(random(10000)) / 10.0f;
  
  if (!weather_.valid) {
    WeatherParams default_params{};
    default_params.temp_f = 65.0f;
    default_params.wind_speed_mph = 5.0f;
    default_params.cloud_cover_pct = 20;
    default_params.valid = true;
    setWeather(default_params);
  }
  updatePalette();
}

void CurlNoiseScene::update(uint32_t dt_ms) {
  // Smoothly transition parameters
  time_speed_ = time_speed_ * 0.95f + target_time_speed_ * 0.05f;
  noise_scale_ = noise_scale_ * 0.95f + target_noise_scale_ * 0.05f;

  z_offset_ += (time_speed_ * dt_ms) / 1000.0f;
}

void CurlNoiseScene::render(Adafruit_Protomatter &matrix) {
  const float epsilon = 0.1f;
  const float inv_2eps = 1.0f / (2.0f * epsilon);

  for (int y = 0; y < kMatrixHeight; ++y) {
    for (int x = 0; x < kMatrixWidth; ++x) {
      float fx = (float)x * noise_scale_;
      float fy = (float)y * noise_scale_;

      // Finite difference to find partial derivatives
      // We sample noise(x, y, z) as our potential field psi
      float psi_x_plus = noise(fx + epsilon, fy, z_offset_);
      float psi_x_minus = noise(fx - epsilon, fy, z_offset_);
      float psi_y_plus = noise(fx, fy + epsilon, z_offset_);
      float psi_y_minus = noise(fx, fy - epsilon, z_offset_);

      float d_psi_dx = (psi_x_plus - psi_x_minus) * inv_2eps;
      float d_psi_dy = (psi_y_plus - psi_y_minus) * inv_2eps;

      // Curl in 2D: V = (d_psi/dy, -d_psi/dx)
      float vx = d_psi_dy;
      float vy = -d_psi_dx;

      // Map vector to color
      // Use angle for hue/index
      float angle = atan2f(vy, vx) + (float)M_PI; // 0 to 2PI
      (void)sqrtf(vx * vx + vy * vy); // Magnitude (unused for now, suppressed warning)

      // Map angle to 0..15 index
      int color_idx = (int)(angle * 16.0f / (2.0f * (float)M_PI)) & 0x0F;
      
      // Optional: use magnitude to influence palette selection or brightness
      // For now, we follow the SPEC: "Non-binary", "Low contrast", "Continuous"
      uint16_t color = palette_[allowed_indices_[color_idx % allowed_count_]];
      
      matrix.drawPixel(x, y, color);
    }
  }
}

void CurlNoiseScene::setWeather(const WeatherParams &params) {
  weather_ = params;
  const float temp_f = params.valid ? params.temp_f : 65.0f;

  // Band-based Palette Selection (Per COLOR.md) - Exact match with FlowFieldScene
  uint8_t allowed[16];
  uint8_t allowed_count = 0;
  
  cold_green_scale_q8_ = 255;
  
  if (temp_f <= 20.0f) {
    const uint8_t set[] = {0, 1, 15}; 
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    cold_green_scale_q8_ = 100;
  } 
  else if (temp_f <= 40.0f) {
    const uint8_t set[] = {0, 1, 2, 3, 15};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    cold_green_scale_q8_ = 140;
  }
  else if (temp_f <= 50.0f) {
    const uint8_t set[] = {0, 1, 2, 3, 4};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
    cold_green_scale_q8_ = 180;
  }
  else if (temp_f <= 65.0f) {
    const uint8_t set[] = {2, 3, 4, 5, 6};
    for(uint8_t i=0; i<sizeof(set); ++i) allowed[allowed_count++] = set[i];
  }
  else if (temp_f <= 80.0f) {
    for(uint8_t i=0; i<16; ++i) allowed[allowed_count++] = i;
  }
  else if (temp_f <= 90.0f) {
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

  uint8_t temp_warm = (uint8_t)PaletteUtils::clampU8((int)(temp_f * 2.55f));
  if (temp_warm != last_temp_warm_) {
    last_temp_warm_ = temp_warm;
    updatePalette();
  }

  // Wind -> Animation Speed
  if (params.valid) {
    target_time_speed_ = 0.05f + (params.wind_speed_mph / 20.0f) * 0.45f;
    if (target_time_speed_ > 1.0f) target_time_speed_ = 1.0f;

    target_noise_scale_ = 0.1f - (params.cloud_cover_pct / 100.0f) * 0.06f;

    if (params.precip_prob_pct > 70) {
      target_noise_scale_ *= 1.2f;
    }
  }
}

void CurlNoiseScene::updatePalette() {
  const int warm_offset = (int)last_temp_warm_ - 128;
  const int red_bias = (warm_offset * 20) / 128;
  const int blue_bias = (warm_offset * -20) / 128;
  const int green_bias = (warm_offset * 5) / 128;

  for (uint8_t i = 0; i < 16; ++i) {
    const int r = (int)PaletteUtils::kBasePaletteRgb[i][0] + red_bias;
    int g = (int)PaletteUtils::kBasePaletteRgb[i][1] + green_bias;
    const int b = (int)PaletteUtils::kBasePaletteRgb[i][2] + blue_bias;
    
    // Apply Cold Green Suppression
    g = (int)(((int32_t)g * (int32_t)cold_green_scale_q8_) / 255);
    
    palette_[i] = PaletteUtils::color565(
      PaletteUtils::clampU8(r), 
      PaletteUtils::clampU8(g), 
      PaletteUtils::clampU8(b)
    );
  }
}

float CurlNoiseScene::noise3D(float x, float y, float z) {
    return noise(x, y, z);
}