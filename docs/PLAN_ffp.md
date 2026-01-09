# Matrix Ambient Weather Viz – Project Plan

## 1. Overview

This project creates an **ambient, generative visual system** designed to run continuously on an **Adafruit Matrix Portal M4 with a 32×64 RGB LED matrix**.

The display is intended for:
- Background use during video calls
- Long-running, non-interactive operation
- Aesthetic “technical art” rather than signage or text

The visuals should feel:
- Alive and emergent
- Calm and non-distracting
- Intentionally designed, not random noise
- Inspired by early artificial life systems (e.g. Game of Life), but more modern

---

## 2. Core Goals

### Functional Goals
- Maintain smooth animation (target: 30–60 FPS where feasible)
- Run indefinitely without memory leaks or instability
- Avoid text, icons, or explicit information displays
- Be visually interesting even when viewed peripherally

### Aesthetic Goals
- Emphasize motion, flow, and emergence
- Use limited, tasteful color palettes
- Avoid obvious loops or resets
- Feel “computationally alive” rather than decorative

### Technical Goals
- Written in **C++ (Arduino framework) using PlatformIO**
- Efficient use of CPU and RAM
- Modular architecture to support multiple visual “scenes”
- Deterministic and debuggable behavior

---

## 3. Platform & Constraints

### Hardware
- Adafruit Matrix Portal M4 (SAMD51)
- 32×64 RGB LED matrix (2048 pixels)

### Constraints
- Limited RAM → avoid large float buffers
- Avoid excessive per-pixel trig or convolution
- Prefer fixed-point or lightweight math where possible
- Minimize dynamic allocation in the render loop

---

## 4. Initial Visual Systems (Planned)

### Primary Scene (Phase 1)
**Flow Field Particles with Trails**

Description:
- A 2D vector field defines direction at each point
- Hundreds of particles move through the field
- Motion leaves fading trails
- Field parameters slowly evolve over time

Why:
- High visual payoff for low computational cost
- Smooth motion reads well on camera
- Feels organic and intentional

---

### Secondary Scenes (Future / Optional)

#### Enhanced Game of Life
- Cell aging and color gradients
- Soft decay instead of binary on/off
- Slowly mutating rule sets

#### Low-Resolution Reaction–Diffusion
- Simulated at reduced resolution
- Upscaled to full display
- Parameters drift slowly

(Only one scene is required for initial completion.)

---

## 5. System Architecture

### High-Level Structure

- **Engine**
  - Owns the framebuffer
  - Handles timing and frame updates
  - Applies global post-processing (fade, blur, palette)

- **Scene Interface**
  - `init()`
  - `update(float dt)`
  - `render(FrameBuffer& fb)`

- **Scenes**
  - Each visual system is a self-contained scene
  - No scene directly accesses hardware

---

### Framebuffer Strategy

- Single primary framebuffer sized 32×64
- Stored as RGB or indexed color (palette-based)
- Each frame:
  1. Apply global fade
  2. Scene renders new contributions
  3. Push framebuffer to display

---

## 6. Flow Field Particle System (Phase 1 Detail)

### Data Structures
- Particle array:
  - Position (fixed-point or float)
  - Velocity or heading
  - Optional lifetime or age
- Vector field:
  - Low resolution (e.g. 16×8 or procedural)
  - Direction derived from noise or simple math

### Update Loop
1. Sample vector field at particle position
2. Update particle direction
3. Move particle
4. Wrap or respawn if out of bounds
5. Draw particle into framebuffer

### Rendering
- Draw particles additively
- Fade framebuffer slightly each frame to create trails
- Optional color based on velocity, age, or position

---

## 7. Timing & Performance

- Fixed timestep or capped variable timestep
- Avoid blocking calls in loop
- Target:
  - ~500 particles
  - 1 full-frame fade pass per frame
  - No heap allocation during animation

---

## 8. Configuration & Tuning

Initial parameters should be compile-time constants:
- Particle count
- Fade rate
- Field scale
- Particle speed

Future extension:
- Runtime tuning via serial or config file
- Time-based parameter drift (no user interaction required)

---

## 9. Development Phases

### Phase 1 – Skeleton
- Display initialization
- Framebuffer abstraction
- Scene interface
- Simple test scene (e.g. moving dots)

### Phase 2 – Flow Field Core
- Vector field generation
- Particle system
- Basic trails

### Phase 3 – Visual Polish
- Color palettes
- Smooth fading
- Parameter drift
- Performance tuning

### Phase 4 – Stability
- Long-run testing
- Memory and timing verification
- Minor aesthetic refinements

---

## 10. Non-Goals (Explicitly Out of Scope)

- Text rendering
- Menus or UI
- User interaction
- Audio-reactive behavior
- Precise data visualization

---

## 11. Definition of Done

The project is considered complete when:
- The display can run for hours without degradation
- Motion is smooth and non-distracting
- Visuals feel intentional and “alive”
- The codebase cleanly supports adding new scenes


