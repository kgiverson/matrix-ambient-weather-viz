#include "net/WeatherClient.h"

#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>

#include "secrets.h"

namespace {
constexpr bool kUseTLS = true;
constexpr char kHost[] = "api.open-meteo.com";
constexpr uint16_t kPort = kUseTLS ? 443 : 80;
constexpr bool kResolveHost = true;
constexpr bool kUseHttpSanity = true;

constexpr uint32_t kFetchIntervalMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kReadTimeoutMs = 2000;
constexpr uint32_t kTotalTimeoutMs = 15000;

constexpr uint32_t kBackoffScheduleMs[] = {60000, 120000, 300000, 600000};
constexpr uint8_t kBackoffMaxIndex =
    (uint8_t)(sizeof(kBackoffScheduleMs) / sizeof(kBackoffScheduleMs[0]) - 1);

constexpr uint16_t kReadChunkMax = 64;
constexpr uint32_t kSmoothTimeMs = 8000;

void formatFixed4(float value, char *out, size_t out_len) {
  if (!out || out_len == 0) {
    return;
  }
  const float scaled_f = value * 10000.0f;
  int32_t scaled = (int32_t)(scaled_f + (scaled_f >= 0.0f ? 0.5f : -0.5f));
  int32_t whole = scaled / 10000;
  int32_t frac = scaled % 10000;
  if (frac < 0) {
    frac = -frac;
  }
  snprintf(out, out_len, "%ld.%04ld", (long)whole, (long)frac);
}

bool canLog() {
  return Serial && Serial.availableForWrite() > 8;
}

void logTimestamp() {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] ");
}

int parseStatusCode(const char *header) {
  if (!header) {
    return -1;
  }
  const char *line = strstr(header, "HTTP/");
  if (!line) {
    return -1;
  }
  const char *space = strchr(line, ' ');
  if (!space) {
    return -1;
  }
  return atoi(space + 1);
}
} // namespace

const char *WeatherClient::stateName(State state) {
  switch (state) {
    case State::kDisconnected:
      return "DISCONNECTED";
    case State::kConnecting:
      return "CONNECTING";
    case State::kIdle:
      return "IDLE";
    case State::kFetchStart:
      return "FETCH_START";
    case State::kSendRequest:
      return "SEND_REQUEST";
    case State::kReadHeaders:
      return "READ_HEADERS";
    case State::kReadBody:
      return "READ_BODY";
    case State::kParse:
      return "PARSE";
    case State::kCoolDown:
      return "COOL_DOWN";
  }
  return "UNKNOWN";
}

WeatherClient::WeatherClient()
    : state_(State::kDisconnected),
      state_start_ms_(0),
      next_fetch_ms_(0),
      request_start_ms_(0),
      connect_start_ms_(0),
      last_io_ms_(0),
      last_smooth_ms_(0),
      backoff_index_(0),
      connect_attempted_(false),
      logged_first_bytes_(false),
      request_len_(0),
      request_sent_(0),
      write_failed_(false),
      write_fail_ms_(0),
      chunked_(false),
      chunk_state_(ChunkState::kReadSize),
      chunk_size_(0),
      chunk_read_(0),
      chunk_line_len_(0),
      chunk_zero_(false),
      content_length_(-1),
      header_len_(0),
      body_len_(0),
      sample_{},
      smoothed_{},
      client_(nullptr),
      ssl_client_(),
      tcp_client_(),
      header_buf_{},
      body_buf_{},
      chunk_line_buf_{},
      request_buf_{} {}

void WeatherClient::begin() {
  sample_.valid = false;
  smoothed_.valid = false;
  state_ = State::kDisconnected;
  next_fetch_ms_ = 0;
  backoff_index_ = 0;
  logged_first_bytes_ = false;
  request_len_ = 0;
  request_sent_ = 0;
  write_failed_ = false;
  write_fail_ms_ = 0;
  chunked_ = false;
  chunk_state_ = ChunkState::kReadSize;
  chunk_size_ = 0;
  chunk_read_ = 0;
  chunk_line_len_ = 0;
  chunk_zero_ = false;
  client_ = kUseTLS ? static_cast<WiFiClient *>(&ssl_client_)
                    : static_cast<WiFiClient *>(&tcp_client_);

#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: client mode ");
    Serial.println(kUseTLS ? "TLS" : "HTTP");
  }
#endif
}

const WeatherClient::WeatherSample &WeatherClient::sample() const {
  return sample_;
}

const WeatherClient::WeatherSample &WeatherClient::smoothed() const {
  return smoothed_;
}

bool WeatherClient::hasSample() const {
  return sample_.valid;
}

void WeatherClient::transition(State next, uint32_t now_ms) {
#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: state ");
    Serial.print(stateName(state_));
    Serial.print(" -> ");
    Serial.println(stateName(next));
  }
#endif
  state_ = next;
  state_start_ms_ = now_ms;
}

void WeatherClient::abortRequest() {
  if (client_) {
    client_->stop();
  }
  connect_attempted_ = false;
  logged_first_bytes_ = false;
  request_len_ = 0;
  request_sent_ = 0;
  write_failed_ = false;
  write_fail_ms_ = 0;
  chunked_ = false;
  chunk_state_ = ChunkState::kReadSize;
  chunk_size_ = 0;
  chunk_read_ = 0;
  chunk_line_len_ = 0;
  chunk_zero_ = false;
  content_length_ = -1;
  header_len_ = 0;
  body_len_ = 0;
}

void WeatherClient::beginRequest(uint32_t now_ms) {
  abortRequest();
  request_start_ms_ = now_ms;
  connect_start_ms_ = now_ms;
  last_io_ms_ = now_ms;
  logged_first_bytes_ = false;
  request_len_ = 0;
  request_sent_ = 0;
  write_failed_ = false;
  write_fail_ms_ = 0;
  chunked_ = false;
  chunk_state_ = ChunkState::kReadSize;
  chunk_size_ = 0;
  chunk_read_ = 0;
  chunk_line_len_ = 0;
  chunk_zero_ = false;

  char lat_buf[16];
  char lon_buf[16];
  formatFixed4(WEATHER_LAT, lat_buf, sizeof(lat_buf));
  formatFixed4(WEATHER_LON, lon_buf, sizeof(lon_buf));

  const int written = snprintf(
      request_buf_, sizeof(request_buf_),
      "GET /v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,"
      "cloud_cover,wind_speed_10m,precipitation_probability&temperature_unit="
      "fahrenheit&wind_speed_unit=mph&timezone=auto HTTP/1.1\r\n"
      "Host: %s\r\n"
      "User-Agent: matrix-agd-viz/1.0\r\n"
      "Accept: application/json\r\n"
      "Accept-Encoding: identity\r\n"
      "Connection: close\r\n"
      "\r\n",
      lat_buf, lon_buf, kHost);

  if (written <= 0 || written >= (int)sizeof(request_buf_)) {
    request_buf_[0] = '\0';
    request_len_ = 0;
  } else {
    request_len_ = (size_t)written;
    request_buf_[request_len_] = '\0';
  }

#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: request prepared len=");
    Serial.println((unsigned long)request_len_);
    const size_t preview_len = request_len_ < 60 ? request_len_ : 60;
    Serial.print("Weather: request preview: ");
    for (size_t i = 0; i < preview_len; ++i) {
      const char c = request_buf_[i];
      if (c >= 32 && c <= 126) {
        Serial.print(c);
      } else {
        Serial.print('.');
      }
    }
    Serial.println();
  }
#endif
}

void WeatherClient::tick(uint32_t now_ms) {
  updateSmoothing(now_ms);

  if (WiFi.status() != WL_CONNECTED) {
    if (state_ != State::kDisconnected) {
      abortRequest();
      transition(State::kDisconnected, now_ms);
    }
    return;
  }

  if (state_ == State::kDisconnected) {
    transition(State::kIdle, now_ms);
    next_fetch_ms_ = now_ms;
  }

  if (state_ == State::kIdle) {
    if ((int32_t)(now_ms - next_fetch_ms_) >= 0) {
      transition(State::kFetchStart, now_ms);
    } else {
      return;
    }
  }

  if (state_ == State::kFetchStart) {
    beginRequest(now_ms);
    transition(State::kConnecting, now_ms);
  }

  if (state_ == State::kConnecting) {
    handleConnect(now_ms);
    return;
  }

  if (state_ == State::kSendRequest) {
    handleSendRequest(now_ms);
    return;
  }

  if (state_ == State::kReadHeaders) {
    handleReadHeaders(now_ms);
    return;
  }

  if (state_ == State::kReadBody) {
    handleReadBody(now_ms);
    return;
  }

  if (state_ == State::kParse) {
    handleParse(now_ms);
    return;
  }

  if (state_ == State::kCoolDown) {
    if ((int32_t)(now_ms - next_fetch_ms_) >= 0) {
      transition(State::kIdle, now_ms);
    }
    return;
  }
}

void WeatherClient::handleConnect(uint32_t now_ms) {
  if (!connect_attempted_) {
    connect_attempted_ = true;
    const uint32_t connect_begin = millis();
    connect_start_ms_ = connect_begin;
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: connect ");
      Serial.print(kHost);
      Serial.print(":");
      Serial.println(kPort);
    }
#endif
    bool ok = false;
    if (client_) {
      if (kUseHttpSanity) {
        IPAddress sanity_ip;
        const int sanity_dns = WiFi.hostByName("example.com", sanity_ip);
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
        if (canLog()) {
          logTimestamp();
          Serial.print("Weather: sanity DNS example.com -> ");
          if (sanity_dns == 1) {
            Serial.println(sanity_ip);
          } else {
            Serial.println("FAILED");
          }
        }
#endif
        if (sanity_dns == 1) {
          WiFiClient sanity_client;
          const bool sanity_ok = sanity_client.connect(sanity_ip, 80);
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
          if (canLog()) {
            logTimestamp();
            Serial.print("Weather: sanity HTTP connect ");
            Serial.println(sanity_ok ? "ok" : "failed");
          }
#endif
          sanity_client.stop();
        }
      }

      if (!kUseTLS && kResolveHost) {
        IPAddress host_ip;
        const int dns_ok = WiFi.hostByName(kHost, host_ip);
#if WEATHER_LOG_ENABLED
        if (canLog()) {
          logTimestamp();
          Serial.print("Weather: DNS ");
          Serial.print(kHost);
          Serial.print(" -> ");
          if (dns_ok == 1) {
            Serial.println(host_ip);
          } else {
            Serial.println("FAILED");
          }
        }
#endif
        if (dns_ok != 1) {
          scheduleFailure(now_ms);
          return;
        }
        ok = client_->connect(host_ip, kPort);
      } else {
        ok = client_->connect(kHost, kPort);
      }
    }
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      const uint32_t connect_end = millis();
      logTimestamp();
      Serial.print("Weather: connect ");
      Serial.print(ok ? "ok" : "failed");
      Serial.print(" (");
      Serial.print((unsigned long)(connect_end - connect_begin));
      Serial.print(" ms, ret=");
      Serial.print(ok ? 1 : 0);
      Serial.println(")");
    }
#endif
    if (!ok) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: connect failed, WiFi status=");
      Serial.println((int)WiFi.status());
    }
#endif
      scheduleFailure(now_ms);
      return;
    }
    transition(State::kSendRequest, now_ms);
    return;
  }

  if ((uint32_t)(now_ms - connect_start_ms_) > kConnectTimeoutMs) {
    scheduleFailure(now_ms);
  }
}

void WeatherClient::handleSendRequest(uint32_t now_ms) {
  if ((uint32_t)(now_ms - request_start_ms_) > kTotalTimeoutMs) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.println("Weather: send timeout");
    }
#endif
    scheduleFailure(now_ms);
    return;
  }

  if (!client_ || request_len_ == 0) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: send invalid client=");
      Serial.print(client_ ? 1 : 0);
      Serial.print(" req_len=");
      Serial.println((unsigned long)request_len_);
    }
#endif
    scheduleFailure(now_ms);
    return;
  }

  if (request_sent_ == 0 && client_ && !client_->connected()) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.println("Weather: send disconnect before write");
    }
#endif
    scheduleFailure(now_ms);
    return;
  }

  if (request_sent_ < request_len_) {
    const bool connected_before = client_->connected();
    const size_t remaining = request_len_ - request_sent_;
    const int written =
        client_->write((const uint8_t *)request_buf_ + request_sent_,
                       remaining);
    const int write_error = client_->getWriteError();
    const bool connected_after = client_->connected();
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: send connected pre=");
      Serial.print(connected_before ? 1 : 0);
      Serial.print(" post=");
      Serial.println(connected_after ? 1 : 0);
      logTimestamp();
      Serial.print("Weather: bytes written ");
      Serial.print(written);
      Serial.print(" total ");
      Serial.print(request_sent_ + (written > 0 ? (size_t)written : 0));
      Serial.print("/");
      Serial.print(request_len_);
      Serial.print(" err=");
      Serial.println(write_error);
    }
#endif
    if (written > 0) {
      request_sent_ += (size_t)written;
      last_io_ms_ = now_ms;
      write_failed_ = false;
      write_fail_ms_ = 0;
    } else {
      if (!write_failed_) {
        write_failed_ = true;
        write_fail_ms_ = now_ms;
      }
      if ((uint32_t)(now_ms - write_fail_ms_) > kReadTimeoutMs ||
          (client_ && !client_->connected() && request_sent_ == 0)) {
#if WEATHER_LOG_ENABLED
        if (canLog()) {
          logTimestamp();
          Serial.print("Weather: send failed after ");
          Serial.print((unsigned long)(now_ms - write_fail_ms_));
          Serial.print(" ms, sent=");
          Serial.println((unsigned long)request_sent_);
        }
#endif
        scheduleFailure(now_ms);
        return;
      }
      return;
    }
  }

  if (request_sent_ >= request_len_) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.println("Weather: sent request");
    }
#endif
    transition(State::kReadHeaders, now_ms);
  }
}

bool WeatherClient::appendHeaderByte(char c) {
  if (header_len_ + 1 >= sizeof(header_buf_)) {
    return false;
  }
  header_buf_[header_len_++] = c;
  header_buf_[header_len_] = '\0';
  return true;
}

bool WeatherClient::appendBodyByte(char c) {
  if (body_len_ + 1 >= sizeof(body_buf_)) {
    return false;
  }
  body_buf_[body_len_++] = c;
  body_buf_[body_len_] = '\0';
  return true;
}

bool WeatherClient::parseContentLength() {
  const char *match = strstr(header_buf_, "Content-Length:");
  if (!match) {
    match = strstr(header_buf_, "content-length:");
  }
  if (!match) {
    content_length_ = -1;
    return true;
  }
  match += strlen("Content-Length:");
  while (*match == ' ') {
    ++match;
  }
  const int value = atoi(match);
  if (value <= 0) {
    content_length_ = -1;
    return false;
  }
  content_length_ = value;
  return true;
}

bool WeatherClient::parseChunked() {
  const char *match = strstr(header_buf_, "Transfer-Encoding:");
  if (!match) {
    match = strstr(header_buf_, "transfer-encoding:");
  }
  if (!match) {
    return false;
  }
  return strstr(match, "chunked") != nullptr ||
         strstr(match, "Chunked") != nullptr;
}

bool WeatherClient::processChunkByte(char c) {
  if (chunk_state_ == ChunkState::kReadSize) {
    if (c == '\n') {
      if (chunk_line_len_ == 0) {
        return true;
      }
      chunk_line_buf_[chunk_line_len_] = '\0';
      chunk_size_ = strtoul(chunk_line_buf_, nullptr, 16);
      chunk_read_ = 0;
      chunk_zero_ = (chunk_size_ == 0);
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
      if (canLog()) {
        logTimestamp();
        Serial.print("Weather: chunk size 0x");
        Serial.print(chunk_line_buf_);
        Serial.print(" (");
        Serial.print((unsigned long)chunk_size_);
        Serial.print(") total=");
        Serial.println((unsigned long)body_len_);
      }
#endif
      chunk_line_len_ = 0;
      if (chunk_size_ == 0) {
        chunk_state_ = ChunkState::kReadDataCr;
      } else {
        chunk_state_ = ChunkState::kReadData;
      }
      return true;
    }
    if (c == '\r') {
      return true;
    }
    if (chunk_line_len_ + 1 >= (uint8_t)sizeof(chunk_line_buf_)) {
      return false;
    }
    chunk_line_buf_[chunk_line_len_++] = c;
    return true;
  }

  if (chunk_state_ == ChunkState::kReadData) {
    if (!appendBodyByte(c)) {
      return false;
    }
    chunk_read_++;
    if (chunk_read_ >= chunk_size_) {
      chunk_state_ = ChunkState::kReadDataCr;
    }
    return true;
  }

  if (chunk_state_ == ChunkState::kReadDataCr) {
    if (c == '\r') {
      chunk_state_ = ChunkState::kReadDataLf;
    }
    return true;
  }

  if (chunk_state_ == ChunkState::kReadDataLf) {
    if (c == '\n') {
      if (chunk_zero_) {
        chunk_state_ = ChunkState::kDone;
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
        if (canLog()) {
          logTimestamp();
          Serial.println("Weather: chunked done (0 chunk)");
        }
#endif
      } else {
        chunk_state_ = ChunkState::kReadSize;
      }
    }
    return true;
  }

  return true;
}

void WeatherClient::handleReadHeaders(uint32_t now_ms) {
  if ((uint32_t)(now_ms - request_start_ms_) > kTotalTimeoutMs) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.println("Weather: header timeout");
    }
#endif
    scheduleFailure(now_ms);
    return;
  }

  uint16_t read_count = 0;
  while (client_ && client_->available() && read_count < kReadChunkMax) {
    const int byte_val = client_->read();
    if (byte_val < 0) {
      break;
    }
    if (!logged_first_bytes_) {
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
      if (canLog()) {
        logTimestamp();
        Serial.print("Weather: first byte 0x");
        Serial.println((uint8_t)byte_val, HEX);
      }
#endif
      logged_first_bytes_ = true;
    }
    read_count++;
    last_io_ms_ = now_ms;
    if (!appendHeaderByte((char)byte_val)) {
      scheduleFailure(now_ms);
      return;
    }
  }

  if (client_ && !client_->connected() && !client_->available() &&
      header_len_ == 0) {
    scheduleFailure(now_ms);
    return;
  }

  const char *end = strstr(header_buf_, "\r\n\r\n");
  if (!end) {
    return;
  }

  const char *body_start = end + 4;
  const size_t header_bytes = (size_t)(body_start - header_buf_);
  const size_t remaining = header_len_ - header_bytes;

#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
  if (canLog()) {
    const char *line_end = strstr(header_buf_, "\r\n");
    if (line_end) {
      const size_t line_len =
          (size_t)(line_end - header_buf_) < 120
              ? (size_t)(line_end - header_buf_)
              : 120;
      char line_buf[121];
      memcpy(line_buf, header_buf_, line_len);
      line_buf[line_len] = '\0';
      logTimestamp();
      Serial.print("Weather: first line ");
      Serial.println(line_buf);
    }
  }
#endif

  const int status = parseStatusCode(header_buf_);
#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: HTTP status ");
    Serial.println(status);
  }
#endif
  if (status < 200 || status >= 300) {
    scheduleFailure(now_ms);
    return;
  }

  chunked_ = parseChunked();
  if (!chunked_ && !parseContentLength()) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.println("Weather: bad content-length");
    }
#endif
    scheduleFailure(now_ms);
    return;
  }

  if (!chunked_) {
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: content-length=");
    Serial.println(content_length_);
  }
#endif
  } else {
#if WEATHER_LOG_ENABLED && WEATHER_LOG_VERBOSE
  if (canLog()) {
    logTimestamp();
    Serial.println("Weather: transfer-encoding chunked");
  }
#endif
  }

  if (remaining > 0) {
    for (size_t i = 0; i < remaining; ++i) {
      if (chunked_) {
        if (!processChunkByte(body_start[i])) {
          scheduleFailure(now_ms);
          return;
        }
      } else {
        if (!appendBodyByte(body_start[i])) {
          scheduleFailure(now_ms);
          return;
        }
      }
    }
  }

  transition(State::kReadBody, now_ms);
}

void WeatherClient::handleReadBody(uint32_t now_ms) {
  if ((uint32_t)(now_ms - request_start_ms_) > kTotalTimeoutMs) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.println("Weather: body timeout");
    }
#endif
    scheduleFailure(now_ms);
    return;
  }

  uint16_t read_count = 0;
  while (client_ && client_->available() && read_count < kReadChunkMax) {
    const int byte_val = client_->read();
    if (byte_val < 0) {
      break;
    }
    read_count++;
    last_io_ms_ = now_ms;
    if (chunked_) {
      if (!processChunkByte((char)byte_val)) {
        scheduleFailure(now_ms);
        return;
      }
    } else {
      if (!appendBodyByte((char)byte_val)) {
        scheduleFailure(now_ms);
        return;
      }
    }
  }

  if (chunked_) {
    if (chunk_state_ == ChunkState::kDone) {
      transition(State::kParse, now_ms);
      return;
    }
  } else if (content_length_ > 0 && body_len_ >= (size_t)content_length_) {
    transition(State::kParse, now_ms);
    return;
  }

  if (client_ && !client_->connected() && !client_->available()) {
    if (chunked_) {
      if (chunk_state_ != ChunkState::kDone) {
        scheduleFailure(now_ms);
        return;
      }
    } else if (body_len_ == 0) {
      scheduleFailure(now_ms);
      return;
    }
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: body read len=");
      Serial.print(body_len_);
      Serial.print(" content_len=");
      Serial.println(content_length_);
    }
#endif
    transition(State::kParse, now_ms);
  }
}

bool WeatherClient::parseFloatField(const char *key, float &out) const {
  const char *match = strstr(body_buf_, key);
  if (!match) {
    return false;
  }
  match = strchr(match, ':');
  if (!match) {
    return false;
  }
  ++match;
  char *endptr = nullptr;
  const float value = strtof(match, &endptr);
  if (endptr == match) {
    return false;
  }
  out = value;
  return true;
}

bool WeatherClient::parseIntField(const char *key, int &out) const {
  float value = 0.0f;
  if (!parseFloatField(key, value)) {
    return false;
  }
  out = (int)(value + 0.5f);
  return true;
}

bool WeatherClient::parseBody(WeatherSample &out) {
  StaticJsonDocument<2048> doc;
  const DeserializationError err = deserializeJson(doc, body_buf_, body_len_);
  if (err) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: JSON error ");
      Serial.println(err.c_str());
    }
#endif
    return false;
  }

  JsonObject current = doc["current"];
  if (current.isNull()) {
    return false;
  }

  const float temp = current["temperature_2m"] | 0.0f;
  const float wind = current["wind_speed_10m"] | 0.0f;
  int cloud = current["cloud_cover"] | 0;
  int precip = current["precipitation_probability"] | 0;

  if (cloud < 0) {
    cloud = 0;
  } else if (cloud > 100) {
    cloud = 100;
  }

  if (precip < 0) {
    precip = 0;
  } else if (precip > 100) {
    precip = 100;
  }

  out.temp_f = temp;
  out.wind_speed_mph = wind;
  out.cloud_cover_pct = (uint8_t)cloud;
  out.precip_prob_pct = (uint8_t)precip;
  return true;
}

void WeatherClient::handleParse(uint32_t now_ms) {
  WeatherSample next = sample_;
  next.valid = false;
  if (body_len_ < sizeof(body_buf_)) {
    body_buf_[body_len_] = '\0';
  } else {
    body_buf_[sizeof(body_buf_) - 1] = '\0';
  }
  if (!parseBody(next)) {
#if WEATHER_LOG_ENABLED
    if (canLog()) {
      logTimestamp();
      Serial.print("Weather: parse failed, body_len=");
      Serial.println(body_len_);
      const size_t snippet_len = body_len_ < 160 ? body_len_ : 160;
      Serial.print("Weather: body snippet: ");
      for (size_t i = 0; i < snippet_len; ++i) {
        const char c = body_buf_[i];
        if (c >= 32 && c <= 126) {
          Serial.print(c);
        } else {
          Serial.print('.');
        }
      }
      Serial.println();
    }
#endif
    scheduleFailure(now_ms);
    return;
  }
  next.sampled_at_ms = now_ms;
  next.valid = true;
  sample_ = next;

  if (!smoothed_.valid) {
    smoothed_ = sample_;
    smoothed_.valid = true;
  }

#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: sample temp_f=");
    Serial.print(sample_.temp_f, 1);
    Serial.print(" cloud=");
    Serial.print(sample_.cloud_cover_pct);
    Serial.print("% wind_mph=");
    Serial.print(sample_.wind_speed_mph, 1);
    Serial.print(" precip=");
    Serial.print(sample_.precip_prob_pct);
    Serial.println("%");
  }
#endif

  scheduleSuccess(now_ms);
}

void WeatherClient::scheduleSuccess(uint32_t now_ms) {
  abortRequest();
  backoff_index_ = 0;
  next_fetch_ms_ = now_ms + kFetchIntervalMs;
#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.println("Weather: fetch ok, scheduling next");
  }
#endif
  transition(State::kCoolDown, now_ms);
}

void WeatherClient::scheduleFailure(uint32_t now_ms) {
  abortRequest();
  const uint8_t index = backoff_index_;
  next_fetch_ms_ = now_ms + kBackoffScheduleMs[index];
  if (backoff_index_ < kBackoffMaxIndex) {
    backoff_index_++;
  }
#if WEATHER_LOG_ENABLED
  if (canLog()) {
    logTimestamp();
    Serial.print("Weather: fetch failed, backoff ");
    Serial.print((unsigned long)kBackoffScheduleMs[index]);
    Serial.println("ms");
  }
#endif
  transition(State::kCoolDown, now_ms);
}

void WeatherClient::updateSmoothing(uint32_t now_ms) {
  if (!sample_.valid) {
    last_smooth_ms_ = now_ms;
    return;
  }

  if (last_smooth_ms_ == 0) {
    last_smooth_ms_ = now_ms;
  }
  const uint32_t dt_ms = now_ms - last_smooth_ms_;
  last_smooth_ms_ = now_ms;

  const float alpha = (dt_ms >= kSmoothTimeMs)
                          ? 1.0f
                          : (float)dt_ms / (float)kSmoothTimeMs;
  smoothed_.temp_f += (sample_.temp_f - smoothed_.temp_f) * alpha;
  smoothed_.wind_speed_mph +=
      (sample_.wind_speed_mph - smoothed_.wind_speed_mph) * alpha;
  float cloud = (float)smoothed_.cloud_cover_pct +
                ((float)sample_.cloud_cover_pct -
                 (float)smoothed_.cloud_cover_pct) *
                    alpha;
  float precip = (float)smoothed_.precip_prob_pct +
                 ((float)sample_.precip_prob_pct -
                  (float)smoothed_.precip_prob_pct) *
                     alpha;
  if (cloud < 0.0f) {
    cloud = 0.0f;
  } else if (cloud > 100.0f) {
    cloud = 100.0f;
  }
  if (precip < 0.0f) {
    precip = 0.0f;
  } else if (precip > 100.0f) {
    precip = 100.0f;
  }
  smoothed_.cloud_cover_pct = (uint8_t)(cloud + 0.5f);
  smoothed_.precip_prob_pct = (uint8_t)(precip + 0.5f);
}
