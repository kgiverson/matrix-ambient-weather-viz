# TASKS – Implementation Checklist (execute in order)

## Phase 0 – Repo & Build Sanity
- [x] Create PlatformIO project (adafruit_matrix_portal_m4 / arduino).
- [x] Add lib_deps for Protomatter, GFX, and Adafruit WiFiNINA fork.
- [x] Add include/secrets.h (gitignored) and include/BoardConfig.h.
- [x] Build + upload a minimal sketch successfully.

## Phase 1 – Display “Known Good”
Goal: prove Protomatter + panel refresh works reliably.
- [x] Initialize Protomatter with BoardConfig pins.
- [x] Show a static test pattern (color bars or checkerboard).
- [x] Confirm no flicker, correct orientation, correct colors.
- [x] Add a simple animation (moving dot) at ~30 FPS.

## Phase 1.2 – Wi-Fi Bringup (ESP32 Coprocessor)
Goal: connect as a client without affecting visuals.
- [x] Add WiFiClient “smoke test”:
      - Connect to SSID/PASS from secrets.h
      - Print status to Serial
      - Retry gracefully
- [x] Ensure render loop continues even if Wi-Fi fails.

## Phase 1.5 – Engine + Scene Skeleton
Goal: establish architecture before complex visuals.
- [x] Implement Scene interface (Scene.h).
- [x] Implement Engine that calls update/render with dt_ms.
- [x] Implement TestScene (bouncing dot or diagonal line).
- [x] Ensure double-buffer swaps are correct and stable.

## Phase 2 – Flow Field Particles (Core)
Goal: particles move smoothly.
- [x] Implement FlowFieldScene with preallocated particle array.
- [x] Implement a cheap vector field:
      - Option A: small grid (e.g., 16×8) with slowly changing angles
      - Option B: procedural pseudo-noise without trig in inner loops
- [x] Particles wrap around edges or respawn.
- [x] Draw particles into buffer.

## Phase 3 – Trails + Fade Post-Process
Goal: “technical art” look.
- [x] Implement global fade pass each frame (multiply brightness).
- [x] Tune fade rate for long trails (video-call friendly).
- [x] Add palette mapping (position/age/speed -> color).

## Phase 4 – Parameter Drift (No Networking Yet)
Goal: non-repeating motion.
- [x] Slowly drift field scale / rotation / turbulence.
- [x] Slowly drift palette phase.
- [x] Verify animation never “resets” or loops obviously.

## Phase 5 – WeatherClient (Scheduled, Time-Budgeted)
Goal: fetch data without freezing.
- [x] Implement WeatherClient as a state machine:
      states: DISCONNECTED, CONNECTING, IDLE, FETCH_START, READ_HEADERS, READ_BODY, PARSE, COOL_DOWN
- [x] Use Open-Meteo (no API key).
- [x] Use lat/lon from include/secrets.h.
- [x] Fetch every 10 minutes; keep last known values.
- [x] Add strict timeouts for connect/read/total.
- [x] Parse only: temperature, cloud cover, wind speed, precip probability.
- [x] Never block the render loop; WeatherClient is a state machine.
- [x] Expose smoothed parameters to FlowFieldScene.

## Phase 6 – Weather Mapping (Ambient, Non-Literal)
Goal: environment affects “mood” not “data display”.
- [x] Map temperature -> palette warmth.
- [x] Map cloud cover -> trail persistence / density.
- [x] Map wind speed -> turbulence.
- [x] Map precip chance -> gravity bias (downward flow).

## Phase 7 – Long-Run Stability
- [ ] Run for 4+ hours.
- [ ] Confirm no memory growth.
- [ ] Confirm no periodic hangs.
- [ ] Confirm Wi-Fi dropouts do not freeze animation.

## Post-Phase Cleanup (Production Readiness)
- [x] Disable verbose WeatherClient logging (`WEATHER_LOG_VERBOSE=0`).
- [x] Decide on WeatherClient logging default (`WEATHER_LOG_ENABLED` on/off).
- [x] Remove or disable the sanity HTTP connect check.
- [ ] Confirm Open-Meteo TLS certs are installed on the WiFiNINA module.
- [ ] Rebuild and upload from a clean tree.
