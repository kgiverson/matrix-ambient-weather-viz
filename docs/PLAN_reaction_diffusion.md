# Plan – Reaction-Diffusion Scene & Persistence

## 1. Overview

This phase introduces a second generative visual system, **Reaction–Diffusion**, and the infrastructure to switch between scenes using a physical button. It also adds non-volatile storage to remember the user's preferred scene across power cycles.

## 2. Goals

### Functional
- **New Scene**: "ReactionDiffusionScene" implementing the Gray-Scott model.
- **Switching**: Single physical button cycles through available scenes.
- **Persistence**: Restore the last active scene on boot using Flash memory.
- **Weather Integration**: Reaction-Diffusion visuals respond to existing weather data (temperature, wind, etc.).

### Technical
- **SceneManager**: A new central coordinator to handle inputs, transitions, and storage, replacing the static scene instances in `main.cpp`.
- **Performance**: The simulation must run at interactive framerates (target 15-30 simulation steps/sec, interpolated or synced to render FPS) on the SAMD51.
- **Storage**: Use `FlashStorage` (EEPROM emulation) to save state without an external SD card.

---

## 3. Architecture Changes

### New Components

1.  **`SceneManager`**
    -   **Responsibility**: Owns the active `Scene` pointer.
    -   **Input**: Polls the button (debounced).
    -   **Storage**: Reads/writes the `PersistentState` struct to Flash.
    -   **Transition**: Calls `scene->begin()` on switch and clears the matrix.

2.  **`ReactionDiffusionScene`**
    -   **State**: Two 2D floating-point (or fixed-point) buffers for chemicals `u` and `v` (ping-pong buffering).
    -   **Logic**: Iterative kernel update (Laplacian) + Reaction terms.
    -   **Render**: Maps chemical `v` concentration to the global color palette.

### Refactoring

-   **`Scene` Interface**:
    -   Promote `WeatherParams` from `FlowFieldScene` to `Scene.h` so all scenes can access weather data via a standardized `setWeather()` method.

---

## 4. Reaction-Diffusion Implementation Details

### Algorithm: Gray-Scott Model
The system uses two grids, $u$ and $v$.
$$
\frac{\partial u}{\partial t} = D_u \nabla^2 u - uv^2 + F(1-u)
$$
$$
\frac{\partial v}{\partial t} = D_v \nabla^2 v + uv^2 - (F+k)v
$$

### Memory Layout
-   **Grid**: 64x32 resolution matches the LED matrix.
-   **Storage**:
    -   Requires 2 grids $\times$ 2 chemicals $\times$ `sizeof(float)` (4 bytes) $\times$ 64 $\times$ 32.
    -   $2 \times 2 \times 4 \times 2048 \approx 32$ KB RAM.
    -   The SAMD51 has ~192KB RAM, so this fits comfortably.

### Optimization Strategy
-   **Ping-Pong Buffers**: Avoid copying data; just swap pointers between `current` and `next` buffers.
-   **Precomputed Laplacians**: (Optional) Use a stencil kernel.
-   **Updates per Frame**: Run multiple simulation steps per render frame if the simulation is too slow visually, or 1 step per frame if CPU limited.

### Visual Mapping
-   **Color**: Use `v` value to look up a color in the palette.
-   **Weather Influence**:
    -   **Temp**: Palette selection (Warm/Cold).
    -   **Wind**: Modulates diffusion rates ($D_u, D_v$) or feed rate ($F$) slightly to create "turbulence".
    -   **Precip**: Triggers random "drops" (inject $v$) into the simulation.

---

## 5. Storage / Persistence Strategy

### Data Structure
```cpp
struct PersistentState {
  uint32_t magic;      // 0xA55A0001
  uint8_t current_scene_id;
  uint8_t reserved[3];
};
```

### Logic
-   **Boot**: Read struct. Check `magic`. If valid, load `current_scene_id`. Else default to 0.
-   **Switch**: When button press confirms a scene change, write the new ID to Flash.
-   **Wear Leveling**: The `FlashStorage` library typically handles page erases. We only write on user interaction (button press), so write cycles are negligible.

---

## 6. Hardware Configuration

-   **Button Pin**: Defined in `BoardConfig.h` (e.g., Pin 2 or 3).
-   **Mode**: `INPUT_PULLUP` (Active Low).
