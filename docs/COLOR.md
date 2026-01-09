# Color ↔ Temperature Mapping (Perceptual)

This document defines the **intentional mapping between outdoor temperature (°F) and color behavior**
for the MatrixPortal FlowField visual.  
It is **perceptual, not linear**, and tuned for US Midwest weather and webcam readability.

The goal is to make the display *legible at a glance* and a subtle conversation piece:
> “Oh — it’s cold outside today.”

This document describes **what the colors should feel like**, not how they are implemented.

---

## Core Principles

1. **Color encodes temperature, not brightness**
   - Cold should not be dim
   - Calm should not be dim
   - Visibility must remain good on camera

2. **Green represents growth, not heat**
   - Green should be rare in cold weather
   - Green peaks in comfortable temperatures
   - Green recedes again in extreme heat

3. **Color variety tracks comfort**
   - Very cold → restrained, low variety
   - Comfortable → richest variety
   - Very hot → reduced variety, warm dominance

4. **Cold should never read as “spring”**
   - Cyan is allowed
   - Spring green is not
   - Cyan must be green-suppressed at low temperatures to avoid false warmth

---

## Temperature Bands & Visual Intent

### ❄️ Very Cold (≤ 20°F)
**Intent:** Winter, inert, austere

- Deep blues, indigo, violet
- Near-monochrome
- No green, no red
- Very low palette variety

**Reads as:**  
> “It’s cold and still.”

---

### 🥶 Cold (20–40°F)
**Intent:** Cold but alive (winter air)

- Blues + icy cyan + muted purples
- Cyan allowed, but **green suppressed**
- No warm tones
- Reduced palette variety

**Rule:**  
Cyan is OK. Spring green is not.

**Reads as:**  
> “Cold day. Nothing growing.”

---

### 🌱 Cool / Early Spring (40–50°F)
**Intent:** Transition phase

- Blues still dominant
- Small hints of muted green may begin
- Still no red
- Palette variety slowly increasing

**Reads as:**  
> “It’s warming, but it’s not warm.”

---

### 🌤️ Mild / Comfortable (50–65°F)
**Intent:** Pleasant, calm, balanced

- Blues and greens in balance
- Natural, relaxed palette
- No aggressive warmth
- High legibility on camera

**Reads as:**  
> “Nice day.”

---

### 🌈 Sweet Spot (70–80°F)
**Intent:** Peak comfort, life, energy

- Full rainbow allowed
- Greens strong and healthy
- Blues, purples, yellows present
- **Highest palette variety**

**This is the only range with full-spectrum color.**

**Reads as:**  
> “Perfect weather.”

---

### 🔥 Hot (80–90°F)
**Intent:** Heat becoming dominant

- Reds and oranges increasing
- Greens receding
- Palette warmth bias rising
- Variety begins to compress

**Reads as:**  
> “It’s getting hot.”

---

### 🥵 Very Hot (≥ 90°F)
**Intent:** Oppressive heat

- Reds and oranges dominate
- Blues and greens mostly suppressed
- Lower variety, intense warmth

**Reads as:**  
> “Too hot.”

---

## Cross-Cutting Rules

### Green Handling
- Green is **suppressed below ~45–50°F**
- Green peaks between **50–75°F**
- Green fades again as red takes over in heat
- Cold cyan must have its green channel damped to avoid “spring” perception

### Brightness
- Brightness is **not** used to encode temperature
- No global dimming for cold or calm conditions
- Camera safety (if needed) should be red-channel compression only, and only in hot ranges

### Palette Variety
- Very cold → low variety
- Comfortable → maximum variety
- Very hot → reduced variety again (but warm)

---

## One-Sentence Mental Model

> **Cold = blue and restrained.  
> Comfortable = balanced and colorful.  
> Hot = red and oppressive.**

If a change violates this intuition, it is probably wrong.

---

## Notes for Implementation (Binding for New Scenes)

To ensure consistency across all visualizations (FlowField, ReactionDiffusion, CurlNoise, etc.), **all new scenes must strictly adhere to the following implementation pattern**:

1.  **Use `PaletteUtils::kBasePaletteRgb`** as the source of truth for base colors.
2.  **Implement Temperature Banding:** Match the thresholds defined above (20, 40, 50, 65, 80, 90) to restrict the `allowed_indices` (the visible subset of the palette).
3.  **Apply Cold Green Suppression:** Explicitly dampen the green channel of cyan/teal colors when temperature is low (< 50°F) to prevent a "spring" look. This is critical for the "Cold" and "Cool" bands.
4.  **Apply Warmth Tinting:** Shift the entire palette's RGB values slightly (Red bias up, Blue bias down) as the calculated "warmth" factor increases.

Do not invent new color schemes or temperature mappings. The users preferred feel is defined by these specific rules.


