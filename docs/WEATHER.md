# Weather Client (Open-Meteo)

Weather data is fetched from Open-Meteo without an API key. Latitude/longitude come from `include/secrets.h`.

Request Format
```
GET https://api.open-meteo.com/v1/forecast?latitude={LAT}&longitude={LON}&current=temperature_2m,cloud_cover,wind_speed_10m,precipitation_probability&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=auto
```

Example JSON Snippet
```json
{
  "current": {
    "temperature_2m": 65.5,
    "cloud_cover": 72,
    "wind_speed_10m": 8.8,
    "precipitation_probability": 35
  }
}
```

Stored Sample
```cpp
struct WeatherSample {
  float temp_f;
  uint8_t cloud_cover_pct;
  float wind_speed_mph;
  uint8_t precip_prob_pct;
  uint32_t sampled_at_ms;
  bool valid;
};
```

Parsing & Transport Notes
- Responses may use `Transfer-Encoding: chunked`; the client de-chunks to a contiguous JSON buffer before parsing.
- JSON parsing uses ArduinoJson with length-aware deserialization.
- Buffer is null-terminated before parsing for safety.

Mapping Table
| Field | Visual Parameter |
| --- | --- |
| temperature_2m | Palette warmth |
| cloud_cover | Trail persistence / density |
| wind_speed_10m | Turbulence |
| precipitation_probability | Gravity bias (downward flow) |

Retry / Backoff
- Hard timeouts per request: connect, read, total.
- On any failure or timeout, enter COOL_DOWN and retry after 60s, then 120s, then 300s, then 600s (cap).
- On success, reset backoff and return to the normal 10-minute fetch interval.
- WeatherClient is a non-blocking state machine; render loop must continue uninterrupted.
