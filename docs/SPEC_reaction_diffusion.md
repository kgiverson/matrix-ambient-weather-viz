# ReactionDiffusionScene Requirements

## Goals
- Add a second ambient visualization scene: **Reaction–Diffusion (Turing / Gray-Scott style)**.
- Keep **FlowFieldScene** as the default scene in code.
- Allow switching scenes with a **physical button**.
- Persist the last-selected scene in **non-volatile storage** so it survives resets/power loss.

## Non-Goals
- No text UI, menus, or signage.
- No internet dependencies beyond the existing Weather client (scene switching must work offline).
- No high-precision scientific simulation; this is technical art optimized for the LED matrix.

---

## Scene Switching

### Scene List
- Scene ID 0: `FlowFieldScene` (default)
- Scene ID 1: `ReactionDiffusionScene`

### Behavior
- On boot:
  - Read `last_scene_id` from non-volatile storage.
  - If valid, start that scene.
  - If missing/invalid, start **FlowFieldScene**.

### Button Input
- Single button cycles scenes:
  - FlowField → ReactionDiffusion → FlowField → ...
- Button handling requirements:
  - Use `INPUT_PULLUP` (active-low) unless hardware dictates otherwise.
  - Debounce (software) required.
  - Short press: cycle to next scene.
  - Optional (nice-to-have): long press resets to default (FlowField) and clears stored preference.

### Persistence Timing
- When a scene switch occurs:
  - Update `last_scene_id` in non-volatile storage.
  - Use a small delay or “commit” mechanism as required by the storage backend.
  - Avoid writing repeatedly while the button is bouncing; only write once per confirmed scene change.

---

## Non-Volatile Storage (EEPROM / Flash Emulation)

### Requirement
- Store 1 byte: `last_scene_id`.

### Implementation Notes (Arduino on SAMD51 / MatrixPortal M4)
- The MatrixPortal M4 (SAMD51) does not have classic EEPROM; use **flash-backed storage**.
- Recommended approach: **Flash storage / EEPROM emulation** compatible with Arduino + PlatformIO.
  - Store a small struct with:
    - `magic` (uint32)
    - `version` (uint8)
    - `scene_id` (uint8)
    - `crc` or simple checksum (optional)
- Must validate data on boot:
  - magic matches
  - scene_id in range
  - (optional) checksum ok

### Write Wear
- Flash has limited write cycles; only write on explicit user scene switch.
- No periodic writes.

---

## ReactionDiffusionScene: Visual + Technical Requirements

### Visual Intent
- Organic, slow-evolving **spots/stripes/blobs** (Turing patterns).
- No literal symbols; should feel like “biology / chemistry” at a distance.
- Should be pleasant as a video-call background (avoid rapid flashing).

### Simulation Model
- Use a 2-field reaction–diffusion system (e.g., Gray-Scott):
  - Two buffers per field: U and V (double-buffering) or ping-pong.
  - Iterative update with diffusion + reaction terms.
- Grid size:
  - Prefer simulation at **native resolution** (64x32) for crispness.
  - Optional: simulate at lower resolution (e.g., 32x16) and scale up for speed.

### Performance Targets
- Maintain stable refresh with the LED matrix (no visible stutter).
- Update RD simulation:
  - Either small number of iterations per frame (e.g., 1–4)
  - Or run RD at a fixed timestep and render every loop.

### Rendering
- Map state to color using the shared palette system:
  - Use `V` concentration (or `V-U`) as the primary scalar.
  - Support palette “mood” from the existing Weather mapping (temperature → palette bias).
- Must support trails/persistence rules only if they don’t conflict:
  - RD is usually self-sustaining; no separate trail fade needed.
  - Rendering should be direct from RD buffers each frame.

### Interaction / Variation
- Initialization / reseeding:
  - On entering the scene: seed the field (random blobs / center dot).
  - Optional: periodic gentle reseed to prevent settling into static patterns.
- Optional weather coupling (phase 2):
  - Temperature: choose parameter set (spots vs stripes).
  - Wind: slightly increase diffusion / iteration rate.
  - Clouds: smooth colors / reduce contrast.
  - Precip: trigger reseed “storm” events.

---

## Architecture Integration

### Scene Interface Expectations
- `init()` called on scene entry
- `update(dt)` called each loop
- `render(matrix)` draws full frame
- `onButton()` or handled centrally by a SceneManager

### SceneManager Requirements
- Owns active scene instance
- Handles button debounce + short press cycling
- Persists new selection to storage
- Calls `activeScene->init()` on switch
- Ensures matrix buffer is cleared when switching scenes

---

## Acceptance Criteria
- Device boots into the last selected scene (verified by power-cycling).
- Button reliably switches scenes without accidental double toggles.
- FlowField remains default when no stored value exists.
- ReactionDiffusionScene runs smoothly and looks organic (spots/stripes/blobs).
- No text overlays; visuals remain “technical art”.


