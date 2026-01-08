matrix-agd-viz is an ambient generative art display for a 32x64 HUB75 panel, running on the Matrix Portal M4.

Weather provider: Open-Meteo (no key), fetch every 10 min.

Hardware
- Adafruit Matrix Portal M4
- 32x64 HUB75 RGB matrix (32px tall)

Build, Upload, Monitor
```sh
pio run
pio run -t upload
pio device monitor -b 115200
```

Configuration
- Wi-Fi credentials and location: `include/secrets.h`
- Matrix pin mapping: `include/BoardConfig.h`

Production Cleanup
- See `docs/TASKS.md` for the post-phase cleanup checklist before long-run deployment.
