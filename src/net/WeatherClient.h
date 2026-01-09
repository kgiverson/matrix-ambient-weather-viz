#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>

#include "AppConfig.h"

#ifndef WEATHER_LOG_ENABLED
#define WEATHER_LOG_ENABLED APP_LOG_WEATHER
#endif

#ifndef WEATHER_LOG_VERBOSE
#define WEATHER_LOG_VERBOSE APP_LOG_WEATHER_VERBOSE
#endif

class WeatherClient {
public:
  struct WeatherSample {
    float temp_f;
    uint8_t cloud_cover_pct;
    float wind_speed_mph;
    uint8_t precip_prob_pct;
    uint32_t sampled_at_ms;
    bool valid;
  };

  WeatherClient();

  void begin();
  void tick(uint32_t now_ms);

  const WeatherSample &sample() const;
  const WeatherSample &smoothed() const;
  bool hasSample() const;

private:
  enum class State : uint8_t {
    kDisconnected,
    kConnecting,
    kIdle,
    kFetchStart,
    kSendRequest,
    kReadHeaders,
    kReadBody,
    kParse,
    kCoolDown
  };

  enum class ChunkState : uint8_t {
    kReadSize,
    kReadData,
    kReadDataCr,
    kReadDataLf,
    kDone
  };

  void transition(State next, uint32_t now_ms);
  void abortRequest();
  void beginRequest(uint32_t now_ms);
  void handleConnect(uint32_t now_ms);
  void handleSendRequest(uint32_t now_ms);
  void handleReadHeaders(uint32_t now_ms);
  void handleReadBody(uint32_t now_ms);
  void handleParse(uint32_t now_ms);
  void scheduleSuccess(uint32_t now_ms);
  void scheduleFailure(uint32_t now_ms);
  void updateSmoothing(uint32_t now_ms);
  static const char *stateName(State state);

  bool appendHeaderByte(char c);
  bool appendBodyByte(char c);
  bool parseContentLength();
  bool parseChunked();
  bool processChunkByte(char c);
  bool parseBody(WeatherSample &out);
  bool parseFloatField(const char *key, float &out) const;
  bool parseIntField(const char *key, int &out) const;

  State state_;
  uint32_t state_start_ms_;
  uint32_t next_fetch_ms_;
  uint32_t request_start_ms_;
  uint32_t connect_start_ms_;
  uint32_t last_io_ms_;
  uint32_t last_smooth_ms_;
  uint8_t backoff_index_;
  bool connect_attempted_;
  bool logged_first_bytes_;
  size_t request_len_;
  size_t request_sent_;
  bool write_failed_;
  uint32_t write_fail_ms_;
  bool chunked_;
  ChunkState chunk_state_;
  uint32_t chunk_size_;
  uint32_t chunk_read_;
  uint8_t chunk_line_len_;
  bool chunk_zero_;

  int content_length_;
  size_t header_len_;
  size_t body_len_;

  WeatherSample sample_;
  WeatherSample smoothed_;

  WiFiClient *client_;
  WiFiSSLClient ssl_client_;
  WiFiClient tcp_client_;
  char header_buf_[1024];
  char body_buf_[2048];
  char chunk_line_buf_[16];
  char request_buf_[512];
};
