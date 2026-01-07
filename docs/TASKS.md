# TASKS – Implementation Checklist (execute in order)

## Phase 0 – Repo & Build Sanity
- [ ] Create PlatformIO project (adafruit_matrix_portal_m4 / arduino).
- [ ] Add lib_deps for Protomatter, GFX, and Adafruit WiFiNINA fork.
- [ ] Add include/secrets.h (gitignored) and include/BoardConfig.h.
- [ ] Build + upload a minimal sketch successfully.

## Phase 1 – Display “Known Good”
Goal: prove Protomatter + panel refresh works reliably.
- [ ] Initialize Protomatter with BoardConfig pins.
- [ ] Show a static test pattern (color bars or checkerboard).
- [ ] Confirm no flicker, correct orientation, correct colors.
- [ ] Add a simple animation (moving dot) at ~30 FPS.

## Phase 1.5 – Engine + Scene Skeleton
Goal: establish architecture before complex visuals.
- [ ] Implement Scene interface (Scene.h).
- [ ] Implement Engine that calls update/render with dt_ms.
- [ ] Implement TestScene (bouncing dot or diagonal line).
- [ ] Ensure double-buffer swaps are correct and stable.

## Phase 2 – Flow Field Particles (Core)
Goal: particles move smoothly.
- [ ] Implement FlowFieldScene with preallocated particle array.
- [ ] Implement a cheap vector field:
      - Option A: small grid (e.g., 16×8) with slowly changing angles
      - Option B: procedural pseudo-noise without trig in inner loops
- [ ] Particles wrap around edges or respawn.
- [ ] Draw particles into buffer.

## Phase 3 – Trails + Fade Post-Process
Goal: “technical art” look.
- [ ] Implement global fade pass each frame (multiply brightness).
- [ ] Tune fade rate for long trails (video-call friendly).
- [ ] Add palette mapping (position/age/speed -> color).

## Phase 4 – Parameter Drift (No Networking Yet)
Goal: non-repeating motion.
- [ ] Slowly drift field scale / rotation / turbulence.
- [ ] Slowly drift palette phase.
- [ ] Verify animation never “resets” or loops obviously.

## Phase 5 – Wi-Fi Bringup (ESP32 Coprocessor)
Goal: connect as a client without affecting visuals.
- [ ] Add WiFiClient “smoke test”:
      - Connect to SSID/PASS from secrets.h
      - Print status to Serial
      - Retry gracefully
- [ ] Ensure render loop continues even if Wi-Fi fails.

## Phase 6 – WeatherClient (Scheduled, Time-Budgeted)
Goal: fetch data without freezing.
- [ ] Implement WeatherClient as a state machine:
      states: DISCONNECTED, CONNECTING, IDLE, FETCH_START, READ_HEADERS, READ_BODY, PARSE, COOL_DOWN
- [ ] Fetch every 10 minutes; keep last known values.
- [ ] Add strict timeouts for connect/read/total.
- [ ] Parse only required fields.
- [ ] Expose smoothed parameters to FlowFieldScene.

## Phase 7 – Weather Mapping (Ambient, Non-Literal)
Goal: environment affects “mood” not “data display”.
- [ ] Map temperature -> palette warmth.
- [ ] Map cloud cover -> trail persistence / density.
- [ ] Map wind speed -> turbulence.
- [ ] Map precip chance -> occasional burst/reset.

## Phase 8 – Long-Run Stability
- [ ] Run for 4+ hours.
- [ ] Confirm no memory growth.
- [ ] Confirm no periodic hangs.
- [ ] Confirm Wi-Fi dropouts do not freeze animation.

