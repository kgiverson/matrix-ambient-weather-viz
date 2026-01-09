#include <Arduino.h>
#include <Adafruit_Protomatter.h>
#include <WiFiNINA.h>

#include "AppConfig.h"
#include "BoardConfig.h"
#include "Engine.h"
#include "SceneManager.h"
#include "net/WeatherClient.h"
#include "scenes/FlowFieldScene.h"
#include "scenes/TestScene.h"
#include "secrets.h"

#ifdef __cplusplus
extern "C" {
#endif
char *sbrk(int i);
#ifdef __cplusplus
}
#endif

static int freeRam() {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}

constexpr uint32_t kFrameIntervalMs = 33; // ~30 FPS
constexpr bool kWiFiSmokeTest = false;
constexpr uint32_t kWiFiSmokeLogIntervalMs = 10000;

enum class WiFiState : uint8_t {
  kIdle,
  kConnecting,
  kConnected,
  kCooldown
};

constexpr uint32_t kWiFiConnectTimeoutMs = 8000;
constexpr uint32_t kWiFiCooldownMs = 5000;
constexpr uint32_t kWiFiStatusIntervalMs = 5000;

static WiFiState wifiState = WiFiState::kIdle;
static uint32_t wifiStateStartMs = 0;
static uint32_t wifiNextAttemptMs = 0;
static uint32_t wifiLastStatusMs = 0;
static uint32_t wifiSmokeLogMs = 0;
static uint32_t heapLogLastMs = 0;

static Engine engine(matrix, kFrameIntervalMs);
static SceneManager sceneManager(matrix, kButtonPin);
static WeatherClient weatherClient;

static void printTimestamp() {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] ");
}

static void printWiFiStatus() {
#if APP_LOG_WIFI
  const uint8_t status = WiFi.status();
  printTimestamp();
  Serial.print("WiFi: status=");
  Serial.print((int)status);
  if (status == WL_CONNECTED) {
    Serial.print(" IP=");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
  }
#endif
}

static void startWiFiConnect(uint32_t nowMs) {
#if APP_LOG_WIFI
  printTimestamp();
  Serial.println("WiFi: begin connect");
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiState = WiFiState::kConnecting;
  wifiStateStartMs = nowMs;
  wifiLastStatusMs = nowMs;
}

static void tickWiFi(uint32_t nowMs) {
  if ((uint32_t)(nowMs - wifiLastStatusMs) >= kWiFiStatusIntervalMs) {
    printWiFiStatus();
    wifiLastStatusMs = nowMs;
  }

  if (wifiState == WiFiState::kConnected) {
    if (WiFi.status() != WL_CONNECTED) {
#if APP_LOG_WIFI
      printTimestamp();
      Serial.println("WiFi: disconnected");
#endif
      wifiState = WiFiState::kCooldown;
      wifiStateStartMs = nowMs;
      wifiNextAttemptMs = nowMs + kWiFiCooldownMs;
    }
    return;
  }

  if (wifiState == WiFiState::kConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
#if APP_LOG_WIFI
      printTimestamp();
      Serial.print("WiFi: connected, IP=");
      Serial.println(WiFi.localIP());
#endif
      wifiState = WiFiState::kConnected;
      return;
    }
    if ((uint32_t)(nowMs - wifiStateStartMs) > kWiFiConnectTimeoutMs) {
#if APP_LOG_WIFI
      printTimestamp();
      Serial.println("WiFi: connect timeout");
#endif
      WiFi.disconnect();
      wifiState = WiFiState::kCooldown;
      wifiStateStartMs = nowMs;
      wifiNextAttemptMs = nowMs + kWiFiCooldownMs;
    }
    return;
  }

  if (wifiState == WiFiState::kCooldown) {
    if ((int32_t)(nowMs - wifiNextAttemptMs) >= 0) {
      wifiState = WiFiState::kIdle;
    }
    return;
  }

  if (wifiState == WiFiState::kIdle) {
    if ((int32_t)(nowMs - wifiNextAttemptMs) >= 0) {
      startWiFiConnect(nowMs);
    }
  }
}

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

  if (!kWiFiSmokeTest) {
    sceneManager.begin();
    engine.setScene(sceneManager.getActiveScene());
    engine.begin();
  }

  weatherClient.begin();
  startWiFiConnect(millis());
}

void loop() {
  const uint32_t nowMs = millis();
  tickWiFi(nowMs);
  weatherClient.tick(nowMs);

#if APP_LOG_HEAP
  if ((uint32_t)(nowMs - heapLogLastMs) >= APP_HEAP_LOG_INTERVAL_MS) {
    printTimestamp();
    Serial.print("System: free RAM = ");
    Serial.println(freeRam());
    heapLogLastMs = nowMs;
  }
#endif

  if (kWiFiSmokeTest) {
    if ((uint32_t)(nowMs - wifiSmokeLogMs) >= kWiFiSmokeLogIntervalMs) {
      printTimestamp();
      Serial.println("WiFi: smoke test heartbeat");
      wifiSmokeLogMs = nowMs;
    }
    return;
  }

  const WeatherClient::WeatherSample &smoothed = weatherClient.smoothed();
  WeatherParams params{};
  params.temp_f = smoothed.temp_f;
  params.wind_speed_mph = smoothed.wind_speed_mph;
  params.cloud_cover_pct = smoothed.cloud_cover_pct;
  params.precip_prob_pct = smoothed.precip_prob_pct;
  params.valid = smoothed.valid;
  sceneManager.setWeather(params);

  sceneManager.tick(nowMs);
  engine.setScene(sceneManager.getActiveScene());
  engine.tick(nowMs);
}
