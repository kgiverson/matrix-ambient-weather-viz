# Agent Instructions (Highest Priority)

These instructions apply to all files in this repository.
If conflicts exist, this file overrides all other documentation.

## Environment
- IDE: VS Code
- Build system: PlatformIO
- Framework: Arduino
- Board: adafruit_matrix_portal_m4

## Build & Upload Commands
- Build: `pio run`
- Upload: `pio run -t upload`
- Serial Monitor: `pio device monitor -b 115200`

## Core Rules (Do Not Violate)
- Never block the render loop on networking or I/O.
- No dynamic memory allocation in per-frame code.
- Preallocate all particle arrays and buffers.
- Keep builds green after each task group.
- Prefer integer or fixed-point math inside hot loops.

## Project Structure
- Matrix pin mapping + matrix instance: `include/BoardConfig.h`
- Wi-Fi credentials and API keys: `include/secrets.h` (gitignored)
- Engine code: `src/Engine.*`
- Scene interface: `src/Scene.h`
- Scenes: `src/scenes/*`
- Networking: `src/net/*`
- Planning docs: `docs/`

## Display & Rendering
- Display driver: Adafruit_Protomatter
- Panel: 32×64 HUB75, 32px tall
- Address lines: A–D (4 lines)
- Double buffering enabled
- Avoid Adafruit_GFX primitives in hot loops; prefer direct pixel writes.

## Networking
- Wi-Fi client mode only.
- Use Adafruit WiFiNINA fork.
- Weather fetch must be scheduled and state-machine driven.
- All network steps must have explicit timeouts.
- If networking fails, visuals must continue uninterrupted.

## Development Process
- Follow tasks in `docs/TASKS.md` in order.
- Implement one task group at a time.
- After each task group:
  - Build with `pio run`
  - Ensure no warnings or errors
  - Confirm runtime behavior matches task description

## Definition of Success
- Flow-field particle animation runs smoothly at ~30–60 FPS.
- No visible stutter or freezing during Wi-Fi activity.
- System remains stable for hours.

