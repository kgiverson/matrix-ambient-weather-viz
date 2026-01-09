# Matrix Ambient Weather Viz

**Matrix Ambient Weather Viz** is an ambient generative art display designed for the [Adafruit Matrix Portal M4](https://www.adafruit.com/product/4745) and 64x32 RGB LED matrices.

I've always been fascinated by blinky lights. I especially like blinky lights that have some sort of method behind their madness. Initially, I put [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) on the board and had it in my office for a number of years. I enjoyed it but always wanted something a bit more reactive to my environment. We all have the weather easily accessible on our phones or computers, but I wanted something that was less of an information display and more part of the environment.

## Features

- **Generative Art Scenes:**
  - **Flow Field:** Particle trails moving through a noise field, influenced by wind speed and direction.
  - **Reaction-Diffusion:** Organic, biological patterns that grow and evolve based on temperature.
  - **Curl Noise:** Fluid-like swirling particles creating complex, non-intersecting paths.
- **Weather Integration:** Uses the [Open-Meteo API](https://open-meteo.com/) (no API key required) to fetch current local conditions every 10 minutes.
- **Dynamic Palettes:** Colors shift automatically based on temperature (cool blues for cold days, warm oranges for hot days).
- **WiFi Connected:** Runs autonomously on the ESP32 co-processor (AirLift) found on the Matrix Portal.

## Controls

- **Cycle Scenes:** Press the **UP** button (on the side of the Matrix Portal) to cycle through the available scenes.
- **Persistence:** Your selected scene is automatically saved to the board's flash memory and will be restored after a reboot or power loss.

## Scene Guide: Weather Reactivity

Each scene interprets the weather data differently to create a unique mood.

| Scene | Wind Speed | Cloud Cover | Temperature | Precipitation |
| :--- | :--- | :--- | :--- | :--- |
| **Flow Field** | Controls particle speed and turbulence. High wind = chaotic motion. | Affects trail length. Cloudy = longer, softer trails. | **Palette:** Shifts from deep winter blues to vibrant summer oranges. | **Gravity:** High rain chance pulls particles downward. |
| **Reaction-Diffusion** | Modulates diffusion rates, causing patterns to morph faster. | Smooths out high-contrast edges for a "foggy" look. | **Pattern:** Cold = spots/blobs. Hot = stripes/mazes. | **Disruption:** Random "raindrops" inject chemicals, disturbing the stable pattern. |
| **Curl Noise** | Scales the time evolution of the field. Faster wind = faster swirls. | Controls the "zoom" level of the noise texture. | **Palette:** Same shared temperature-based color system. | **Distortion:** Storms trigger random jumps in the noise field. |

## Hardware Requirements

1.  **Adafruit Matrix Portal M4** (CircuitPython or Arduino compatible, this project uses Arduino/C++).
2.  **64x32 HUB75 RGB LED Matrix** (P4 or P5 pitch recommended).
3.  **5V 4A+ Power Supply** (USB-C for the Matrix Portal is usually sufficient for average brightness, but a dedicated DC jack is recommended for full white).

## Software Requirements

This project is built using **PlatformIO**, a robust ecosystem for embedded development. I use the command line setup, but they also have a great extension for VS Code.

1.  [**PlatformIO**](https://platformio.org) (CLI or IDE Extension).

*Note: You do not need the standard Arduino IDE installed.*

## Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/kgiverson/matrix-ambient-weather-viz.git
cd matrix-ambient-weather-viz
```

### 2. Configure Secrets
To protect your Wi-Fi credentials, this project uses a `secrets.h` file that is excluded from version control.

1.  Navigate to the `include/` directory.
2.  Copy `secrets_template.h` to a new file named `secrets.h`.
3.  Edit `secrets.h` with your Wi-Fi details and location:

```cpp
// include/secrets.h
#define WIFI_SSID "MyWiFiNetwork"
#define WIFI_PASS "MyPassword123"

// Latitude/Longitude for weather (e.g., New York City)
#define WEATHER_LAT 40.7128f
#define WEATHER_LON -74.0060f
```

### 3. Build and Upload

#### Option A: VS Code
1.  Open the project folder in **VS Code**.
2.  Wait for PlatformIO to initialize (you'll see a toolbar at the bottom).
3.  Connect your Matrix Portal M4 via USB.
4.  Click the **PlatformIO icon** (Alien face) on the left sidebar.
5.  Under **Project Tasks**, choose **adafruit_matrix_portal_m4**:
    - **General > Build** (to compile)
    - **General > Upload** (to flash the firmware)

#### Option B: Terminal (CLI)
```bash
pio run -t upload
```

### 4. Monitor Output
To see logs, Wi-Fi status, and weather updates:
```bash
pio device monitor -b 115200
```

## How It Works

The system runs a game loop engine that manages different `Scene` objects. A separate `WeatherClient` handles network operations.

- **Engine:** Handles the frame rate (~30 FPS) and transitions.
- **SceneManager:** Rotates through active scenes and propagates weather data.
- **WeatherClient:** Operates as a state machine to fetch data. While data reading is asynchronous to minimize frame drops, the initial connection phase may briefly pause the animation (standard behavior for the WiFiNINA library).
- **Protomatter:** The low-level library from Adafruit that handles the complex timing required to drive HUB75 panels.

### Further Reading

For more detailed technical specifications and design philosophy, check the `docs/` folder:

- [**COLOR.md**](docs/COLOR.md): The philosophy behind the perception-based temperature-to-color mapping.
- [**SPEC_ffp.md**](docs/SPEC_ffp.md): Specification for the Flow Field Particles scene.
- [**SPEC_reaction_diffusion.md**](docs/SPEC_reaction_diffusion.md): Details on the Gray-Scott simulation.
- [**SPEC_curl_noise.md**](docs/SPEC_curl_noise.md): Math and implementation of the Curl Noise fluid simulation.

## Customization

- **Board Config:** Pin mappings for the matrix are defined in `include/BoardConfig.h`.
- **App Config:** Logging levels and feature flags are in `include/AppConfig.h`.

## License

MIT License - feel free to fork, modify, and use in your own projects.
