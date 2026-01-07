#include <Arduino.h>
#include <Adafruit_Protomatter.h>

#include "BoardConfig.h"

constexpr uint8_t kMatrixHeight = 32;
constexpr uint32_t kFrameIntervalMs = 33; // ~30 FPS

static void drawTestPattern() {
  matrix.fillScreen(0);

  constexpr uint8_t kBars = 8;
  constexpr uint8_t kBarWidth = kMatrixWidth / kBars;

  const uint16_t colors[kBars] = {
    matrix.color565(255, 0, 0),
    matrix.color565(255, 128, 0),
    matrix.color565(255, 255, 0),
    matrix.color565(0, 255, 0),
    matrix.color565(0, 255, 255),
    matrix.color565(0, 128, 255),
    matrix.color565(0, 0, 255),
    matrix.color565(255, 0, 255)
  };

  for (uint8_t i = 0; i < kBars; ++i) {
    const uint8_t x = i * kBarWidth;
    matrix.fillRect(x, 0, kBarWidth, kMatrixHeight, colors[i]);
  }

  matrix.show();
}

void setup() {
  delay(300);
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  ProtomatterStatus status = matrix.begin();
  Serial.print("matrix.begin status=");
  Serial.println((int)status);
  if (status != PROTOMATTER_OK) {
    while (1) {
      delay(10);
    }
  }

  drawTestPattern();
  delay(1200);
  matrix.fillScreen(0);
  matrix.show();
}

void loop() {
  static uint32_t lastFrameMs = 0;
  static int8_t dx = 1;
  static int8_t dy = 1;
  static int16_t x = 0;
  static int16_t y = 0;

  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastFrameMs) < kFrameIntervalMs) {
    return;
  }
  lastFrameMs = nowMs;

  matrix.fillScreen(0);
  matrix.drawPixel(x, y, matrix.color565(0, 200, 40));
  matrix.show();

  x += dx;
  y += dy;

  if (x <= 0 || x >= (kMatrixWidth - 1)) {
    dx = -dx;
  }
  if (y <= 0 || y >= (kMatrixHeight - 1)) {
    dy = -dy;
  }
}
