#pragma once

// Application Configuration & Feature Flags

// Enable Serial logging for Wi-Fi state changes
#define APP_LOG_WIFI 0

// Enable Serial logging for WeatherClient
#define APP_LOG_WEATHER 1

// Enable verbose (byte-level) logging for WeatherClient
#define APP_LOG_WEATHER_VERBOSE 0

// Enable periodic heap memory monitoring
#define APP_LOG_HEAP 1
#define APP_HEAP_LOG_INTERVAL_MS 60000
