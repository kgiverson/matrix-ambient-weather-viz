# TASKS – Curl Noise Scene

## Phase 1: Foundations & Noise
- [x] **Noise Implementation**:
    - [x] Create a lightweight Simplex Noise implementation (likely inside `CurlNoiseScene.cpp` or a helper `src/SimplexNoise.h`).
    - [x] Implement `float noise(float x, float y, float z)`.
- [x] **Scene Skeleton**:
    - [x] Create `src/scenes/CurlNoiseScene.h` and `.cpp`.
    - [x] Implement `begin()`, `update()`, `render()` stubs.
    - [x] Verify compilation.

## Phase 2: Core Rendering (The "Curl")
- [x] **Field Generation**:
    - [x] Implement the finite difference logic to calculate partial derivatives (`dx`, `dy`).
    - [x] Compute the curl vector `(dy, -dx)` for each pixel.
- [x] **Basic Visualization**:
    - [x] Render a debug view: Map vector angle to Hue (HSV) or a simple RGB ramp.
    - [x] Verify that the pattern looks "swirly" and continuous, not blocky.
- [x] **Time Evolution**:
    - [x] Add a `z` parameter that increments every frame.
    - [x] Verify the field evolves smoothly over time.

## Phase 3: Visual Polish & Palette
- [x] **Palette Mapping**:
    - [x] Import `PaletteUtils.h`.
    - [x] Map the vector angle/magnitude to the shared 16-color palette.
    - [x] Implement smooth interpolation between palette entries if needed.
- [x] **Aesthetics**:
    - [x] Tune noise frequency (scale) for the 64x32 grid.
    - [x] Tune time increments for a "calm" feel.

## Phase 4: Weather Integration
- [x] **Connect Weather**:
    - [x] Implement `setWeather(const WeatherParams& params)`.
    - [x] Map `wind_speed` to time increment (faster wind = faster flow).
    - [x] Map `temperature` to palette selection (Cool/Warm).
    - [x] Map `cloud_cover` or `precip` to noise scale or offsets.
- [x] **Precipitation Event**:
    - [x] Add logic to detect a precip "trigger" (or high probability) and distort the field.

## Phase 5: Scene Manager Integration
- [x] **Register Scene**:
    - [x] Update `SceneManager.h` to include `CurlNoiseScene`.
    - [x] Update `SceneManager.cpp`:
        - [x] Increment `kSceneCount`.
        - [x] Add `CurlNoiseScene` to the `switchScene` logic.
- [x] **Persistence**:
    - [x] Verify scene ID 2 is saved/loaded correctly from Flash.

## Phase 6: Final Verification
- [ ] **Performance**: Ensure >30 FPS. If slow, implement the low-res optimization.
- [ ] **Visual Check**:
    - [ ] No particles?
    - [ ] "Fabric/Smoke" feel?
    - [ ] Legible on camera?
