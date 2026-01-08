# SPEC – Ambient Generative Display (Matrix Portal M4)

## Hardware
- Board: Adafruit Matrix Portal M4 (SAMD51 + ESP32 Wi-Fi coprocessor)
- Panel: 32×64 HUB75 RGB matrix (32px tall, 64px wide)

## Platform
- PlatformIO
- Framework: Arduino
- Display library: Adafruit_Protomatter + Adafruit_GFX
- Wi-Fi library: Adafruit WiFiNINA fork (Git dependency)

## Performance Targets
- Target frame rate: 30–60 FPS (best-effort)
- Never block the render loop on networking or I/O.
- No dynamic allocation in the hot loop (`loop()` or per-frame update/render).
- Avoid large float buffers; prefer integers / fixed-point where practical.

## Visual System (Phase 1)
Scene: Flow Field Particles with Trails
- 300–900 particles (compile-time constant)
- Trails created via a framebuffer fade pass each frame
- Particle motion driven by a low-cost vector field
- Slow parameter drift over time

## Weather Reactivity (Phase 2)
- Provider: Open-Meteo (no API key required).
- Device connects to local Wi-Fi as a client.
- Fetch cadence: no more than once every 10 minutes.
- Weather affects *parameters*, not explicit text/graphics.
- Responses may be chunked; WeatherClient de-chunks to a contiguous JSON buffer.
- Open-Meteo fields requested (exact):
  - `temperature_2m`
  - `wind_speed_10m`
  - `cloud_cover`
  - `precipitation_probability`
- Visual mapping:
  - `temperature_2m` -> palette warmth (cool to warm shift)
  - `cloud_cover` -> trail persistence / density
  - `wind_speed_10m` -> turbulence / flow strength
  - `precipitation_probability` -> occasional burst/reset events

## Networking Rules
- Networking MUST be time-budgeted:
  - Each fetch has a hard overall time limit (e.g., 3–5 seconds).
  - Each step has explicit timeouts (connect/read/parse).
  - If a step times out, abort and retry later.
- Networking MUST be structured as a state machine:
  - Never do a long blocking fetch in a single frame.
- Rendering must continue uninterrupted if the network fails.
- Cache last known weather and continue visuals if offline.

## Code Architecture

### Files / Modules
- src/main.cpp
- include/BoardConfig.h   (matrix pins + matrix instance)
- include/secrets.h       (SSID/PASS/API key; NOT committed)
- src/Engine.h / Engine.cpp
- src/Scene.h
- src/scenes/FlowFieldScene.h / .cpp
- src/net/WeatherClient.h / .cpp

### Interfaces

#### Scene interface
- `void init();`
- `void update(uint32_t dt_ms);`
- `void render();`  (renders into framebuffer / matrix backbuffer)

#### Engine responsibilities
- Owns timing
- Calls `scene.update(dt)` and `scene.render()`
- Applies global fade post-process (or scene does it)
- Pushes final buffer to the display (double buffered)

#### WeatherClient responsibilities
- Connects Wi-Fi (client mode)
- Scheduled fetch (every N minutes)
- Parses only needed fields
- Exposes smoothed parameters (targets + easing)

## Protomatter Configuration
Use known-good pin mapping from prior project.

Panel assumptions:
- 32px tall -> 4 address lines (A–D)
- Width 64
- Bitplanes: 4
- Chains: 1
- Double buffer: true

## “Done” Criteria (Phase 1)
- Stable flow-field particle animation runs for hours
- No visible stutter / hangs
- No memory growth
- Visual is calm + interesting on camera

## “Done” Criteria (Phase 2)
- Device reliably connects to Wi-Fi on boot
- Weather updates modify visual parameters without freezing animation
- If Wi-Fi is unavailable, visuals continue and system retries gracefully
