# Weather Client (Open-Meteo)

Weather data is fetched from Open-Meteo without an API key. Latitude/longitude come from `include/secrets.h`.

Request Format
```
GET https://api.open-meteo.com/v1/forecast?latitude={LAT}&longitude={LON}&current=temperature_2m,cloud_cover,wind_speed_10m,precipitation_probability&timezone=auto
```

Example JSON Snippet
```json
{
  "current": {
    "temperature_2m": 18.6,
    "cloud_cover": 72,
    "wind_speed_10m": 14.1,
    "precipitation_probability": 35
  }
}
```

Stored Sample
```cpp
struct WeatherSample {
  float temp_c;
  uint8_t cloud_cover_pct;
  float wind_speed_kph;
  uint8_t precip_prob_pct;
  uint32_t sampled_at_ms;
};
```

Mapping Table
| Field | Visual Parameter |
| --- | --- |
| temperature_2m | Palette warmth |
| cloud_cover | Trail persistence / density |
| wind_speed_10m | Turbulence |
| precipitation_probability | Occasional burst/reset |

Retry / Backoff
- Hard timeouts per request: connect, read, total.
- On any failure or timeout, enter COOL_DOWN and retry after 60s, then 120s, then 300s, then 600s (cap).
- On success, reset backoff and return to the normal 10-minute fetch interval.
- WeatherClient is a non-blocking state machine; render loop must continue uninterrupted.
