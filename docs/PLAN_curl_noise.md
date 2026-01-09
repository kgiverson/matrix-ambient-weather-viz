# Plan – Curl Noise Scene

## 1. Overview

This phase introduces the **CurlNoiseScene**, a visual system that renders a continuous, fluid-like vector field using curl noise. Unlike the discrete particles of the `FlowFieldScene`, this scene visualizes the field itself as a moving texture, resembling flowing fabric, smoke, or magnetic fields.

## 2. Goals

### Functional
- **New Scene**: `CurlNoiseScene`, accessible via the existing `SceneManager`.
- **Visuals**: A calm, continuous, non-particulate animation.
- **Weather Integration**: Speed and texture modulation based on wind, cloud cover, and precipitation.

### Technical
- **Noise Generation**: Implement a lightweight, spatially coherent noise function (Simplex or Perlin) optimized for the SAMD51 (Cortex-M4F).
- **Performance**: Target 30+ FPS at native resolution (64x32).
- **Memory**: Minimal RAM usage (calculated per-pixel on the fly or using small buffers).

---

## 3. Core Technique

### The Algorithm: Curl Noise
Standard noise (Perlin/Simplex) creates a scalar field. Taking the gradient of this field creates a vector field that points "uphill". However, this looks like separate blobs.

**Curl Noise** takes a scalar potential field $\psi(x, y, t)$ and calculates its curl to generate a velocity field $\vec{v}$. In 2D, this is effectively:
$$ \vec{v} = \nabla \times \psi = \left( \frac{\partial \psi}{\partial y}, -\frac{\partial \psi}{\partial x} \right) $$

**Properties:**
- **Divergence-Free**: The resulting flow has no sinks or sources; it loops and swirls endlessly.
- **Continuous**: No sudden edges (if the source noise is smooth).

### Implementation Steps
1.  **Source Noise**: Implement 3D Simplex Noise `Noise(x, y, time)`.
2.  **Finite Difference**: Approximate derivatives by sampling noise at small offsets:
    $$ \frac{\partial \psi}{\partial x} \approx \frac{N(x+\epsilon, y) - N(x-\epsilon, y)}{2\epsilon} $$
    $$ \frac{\partial \psi}{\partial y} \approx \frac{N(x, y+\epsilon) - N(x, y-\epsilon)}{2\epsilon} $$
3.  **Vector Mapping**: Construct $\vec{v}$.
4.  **Color Mapping**: Map the vector's angle (direction) and magnitude to the active color palette.

---

## 4. Architecture Changes

### New Components
1.  **`CurlNoiseScene` Class**:
    -   Encapsulates the rendering loop and noise state.
    -   **State**: `z_offset` (time), `wind_speed_accumulator`, palette caches.
    -   **Private Helper**: `float noise(x, y, z)` implementation (Simplex).

### Updates
1.  **`SceneManager`**:
    -   Increase `kSceneCount` to 3.
    -   Instantiate `CurlNoiseScene`.
    -   Add switching logic for the new scene ID.

---

## 5. Visual & Weather Integration

### Palette
Reuse `PaletteUtils`. The scene will select colors based on the calculated vector angle, creating smooth gradients across the display.

### Weather Reactivity
| Weather Param | Effect | Implementation |
| :--- | :--- | :--- |
| **Wind Speed** | Animation Speed | Scales the increment of the `z` (time) coordinate in noise. |
| **Cloud Cover** | Smoothness / Scale | Modifies the spatial frequency (zoom) or domain warping. |
| **Temperature** | Color Palette | Selects Cold/Comfortable/Hot palette (via existing logic). |
| **Precipitation** | Distortion / Reset | Triggers random jumps in `z` or `x/y` offsets to "shake" the field. |

---

## 6. Performance Strategy

### Native Resolution (64x32)
-   Total pixels: 2048.
-   Per-pixel cost: ~3 noise evaluations (center/offset for derivatives) or optimized to 2 lookups if using central difference isn't strictly necessary (forward difference is cheaper).
-   **Optimization**: The SAMD51 runs at 120MHz with an FPU. 3D Simplex noise is computationally non-trivial but likely feasible for 2048 pixels.

### Fallback Strategy (Low-Res)
If native rendering drops below 30 FPS:
1.  Compute field at 32x16 (half resolution).
2.  Bilinear interpolate colors up to 64x32.

---

## 7. Development Phases

1.  **Foundation**: Implement Simplex Noise and basic render loop.
2.  **Curl Logic**: Implement the curl math and visualize vectors (e.g., as greyscale or simple RGB).
3.  **Palette Integration**: Map vectors to the "Chicago-centric" palette.
4.  **Weather Connection**: Hook up weather parameters to animation variables.
5.  **Scene Integration**: Add to `SceneManager` and enable button switching.
