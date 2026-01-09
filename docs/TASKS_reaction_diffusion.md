# TASKS – Reaction-Diffusion & Persistence

## Phase 1: Configuration & Infrastructure
- [ ] **Dependencies**: Add `cmaglie/FlashStorage` to `platformio.ini`.
- [ ] **Hardware**: Define `kButtonPin` in `include/BoardConfig.h` and `src/BoardConfig.cpp`.
- [ ] **Refactor Scene Interface**:
    - [ ] Move `WeatherParams` struct definition from `FlowFieldScene.h` to `Scene.h`.
    - [ ] Add virtual `setWeather(const WeatherParams& params)` to base `Scene` class.
    - [ ] Update `FlowFieldScene` to use the base struct.

## Phase 2: SceneManager & Persistence
- [ ] **Storage Logic**:
    - [ ] Define `PersistentState` struct (magic number + scene ID).
    - [ ] Create helper functions to save/load state using `FlashStorage`.
- [ ] **SceneManager Class**:
    - [ ] Create `src/SceneManager.h` and `src/SceneManager.cpp`.
    - [ ] Implement `init()`: Load state, instantiate initial scene.
    - [ ] Implement `tick()`: Handle button debounce, cycle scenes, save state.
    - [ ] Implement `update()` / `render()` delegation.
- [ ] **Main Integration**:
    - [ ] Replace static `FlowFieldScene` in `main.cpp` with `SceneManager`.
    - [ ] Pass button pin events to Manager (or have Manager poll).

## Phase 3: Reaction-Diffusion Core
- [ ] **Skeleton**:
    - [ ] Create `src/scenes/ReactionDiffusionScene.h` / `.cpp`.
    - [ ] Implement `Scene` interface methods.
- [ ] **Simulation Engine**:
    - [ ] Allocate `u` and `v` double-buffers (arrays of floats).
    - [ ] Implement `reset()`: Clear buffers, seed with random spots.
    - [ ] Implement `step()`: Gray-Scott diffusion/reaction math.
- [ ] **Rendering**:
    - [ ] Map `v` concentration to `matrix.drawPixel`.
    - [ ] Use a simple monochrome palette (or existing `AppConfig` palette) for initial test.

## Phase 4: Visual Polish & Weather
- [ ] **Palette Integration**:
    - [ ] Port the "heatmap" logic from `FlowFieldScene` to be reusable or copy it.
    - [ ] Render RD state using the temperature-sensitive palette.
- [ ] **Parameter Tuning**:
    - [ ] Tune $F$ (Feed) and $k$ (Kill) rates for stable spots/worms.
    - [ ] Experiment with simulation speed (steps per frame).
- [ ] **Weather Reactivity**:
    - [ ] Modulate simulation parameters ($F$, $k$, diffusion) based on `WeatherParams`.
    - [ ] Trigger "rain" (random drops) if `precip_prob_pct` is high.

## Phase 5: Verification
- [ ] **Boot Test**: Verify device remembers the selected scene after power cycle.
- [ ] **Button Test**: Verify clean cycling without bouncing.
- [ ] **Stability**: Run RD scene for >1 hour to ensure no memory leaks or numerical explosions.
