# CurlNoiseScene Requirements

## Overview
The CurlNoiseScene visualizes a **continuous vector field texture** derived from curl noise.
It produces smooth, swirling motion that feels like **fabric, smoke, or a flowing material**,
without discrete particles or agents.

This scene should be:
- Calm
- Continuous
- Non-literal
- Highly readable on camera

It complements FlowField (particles) and ReactionDiffusion (chemistry) by rendering the
**field itself**.

---

## Visual Intent

**Primary feel:**
- Moving texture
- Coherent motion
- Soft, organic flow

**Metaphors (non-literal):**
- Flowing cloth
- Magnetic field lines
- Atmospheric currents
- Distorted space

**Anti-goals:**
- No dots / particles
- No text
- No literal symbols
- No rapid flicker or noise

---

## Core Technique

### Vector Field
- Generate a 2D vector field using **curl noise**:
  - Derived from a scalar noise function (e.g. Perlin / Simplex)
  - Curl ensures divergence-free flow (no sinks/sources)
- Field must be:
  - Continuous in space
  - Slowly evolving in time

### Sampling
- For each pixel:
  - Sample the vector field at `(x, y, t)`
  - Use vector magnitude and/or direction to determine color

### Motion
- Motion is implicit:
  - Texture appears to flow due to time evolution of the field
  - No discrete entities moving across the screen

---

## Resolution & Performance

- Prefer rendering at **native resolution** (64×32)
- Optional optimization:
  - Compute vector field at lower resolution (e.g. 32×16) and interpolate
- Must run smoothly alongside Protomatter refresh
- Avoid per-pixel heavy math inside tight loops where possible

---

## Color & Palette

### Palette Source
- Reuse the **shared temperature-based palette system**
  (same one used by FlowField and ReactionDiffusion).

### Color Mapping
- Color may be derived from:
  - Vector direction (angle)
  - Vector magnitude
  - Noise value used to generate curl
- Color should feel:
  - Continuous
  - Low contrast
  - Non-binary

### Temperature Influence
- Cold → cooler palettes, reduced variety
- Comfortable → richer color range
- Hot → warmer bias

No brightness dimming based on temperature.

---

## Weather Reactivity (Implemented)

### Wind $\rightarrow$ Flow Velocity
- Wind speed (mph) scales the rate of time evolution (Z-axis of noise).
- **Calm wind**: Slow, almost static drift.
- **High wind**: Pronounced swirling and energetic motion.

### Clouds $\rightarrow$ Texture Scale
- Cloud cover (%) controls the spatial frequency of the noise.
- **High cloud**: Larger, smoother, "pillowy" features.
- **Low cloud**: Higher detail, finer currents, more "crunchy" texture.

### Temperature $\rightarrow$ Palette & Tint
- **Banding**: Matches shared project thresholds (20, 40, 50, 65, 80, 90°F) to restrict the `allowed_indices` of the palette.
- **Tinting**: Applies global RGB shifts (Red bias for heat, Blue bias for cold).
- **Green Suppression**: Actively damps green channel in cold weather (< 50°F) to preserve winter aesthetic.

### Precipitation $\rightarrow$ Turbulence
- High precipitation probability (> 70%) triggers a "busy" multiplier (1.2x) on the noise scale.
- Does NOT draw literal rain; creates a more chaotic or "disturbed" flow field.

---

## Scene Lifecycle

### Initialization
- On scene entry:
  - Initialize noise offsets and time
  - Clear display buffer
  - Start in a calm, readable state

### Update Loop
- Advance noise time using:
  - Small base rate
  - Wind-scaled component
- Avoid sudden jumps unless triggered by precipitation

### Rendering
- Full-frame render every frame
- No trail buffer required (unless deliberately added later)

---

## Integration Requirements

### Scene Identity
- Scene ID: next available after ReactionDiffusion
- Name: `CurlNoiseScene`

### SceneManager
- Must support switching to/from CurlNoiseScene via button
- Must persist selected scene ID using existing storage mechanism

### Shared Systems
- Use existing:
  - Weather data
  - Palette logic
  - SceneManager lifecycle (`init`, `update`, `render`)

---

## Acceptance Criteria

- Scene reads as a continuous flowing texture
- No visible particles or dots
- Calm at low wind, expressive at higher wind
- Legible and pleasant on camera
- Switching scenes clears prior visual state cleanly
- Scene selection persists across reboot

---

## Intent Summary

> This scene visualizes the *field itself*, not objects moving through it.

If it ever looks like particles, noise, or static,
it is moving away from its intended purpose.

