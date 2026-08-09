# rgb-node

3-channel PWM RGB LED driver firmware with INMP441 I2S Music Sync and Home Assistant MQTT for the ESP32-C3 Super Mini.

---

## Hardware Pinout Map

| ESP32-C3 Pin | Function | Peripheral / Description |
| :--- | :--- | :--- |
| `GPIO 5` | PWM Red Output | Red LED Channel PWM (5 kHz, 12-bit) |
| `GPIO 6` | PWM Green Output | Green LED Channel PWM (5 kHz, 12-bit) |
| `GPIO 7` | PWM Blue Output | Blue LED Channel PWM (5 kHz, 12-bit) |
| `GPIO 8` | Onboard LED | Active-Low LED (mirrors Red PWM) |
| `GPIO 2` | I2S `BCLK` / `SCK` | INMP441 MEMS Mic Serial Bit Clock |
| `GPIO 3` | I2S `WS` / `LCK` | INMP441 MEMS Mic Word Select / Frame Clock |
| `GPIO 4` | I2S `SD` / `DIN` | INMP441 MEMS Mic Data Out $\rightarrow$ ESP32 Data In |
| `GPIO 1` | ADC1 Input | Optional Hardware Potentiometer Wiper (`RGBNODE_BRIGHTNESS_POT = 0`) |
| `3.3V` / `GND` | Power Rail | INMP441 Power (Mic L/R tied to GND for Left Channel) |

---

## Prerequisites

- [Node.js (v18+)](https://nodejs.org/) & `npm`
- [Python (3.9+)](https://www.python.org/) or [uv](https://docs.astral.sh/uv/getting-started/installation/) — used to install PlatformIO (`uv tool install platformio` or `pip install platformio`).
- Optional: `make` (for Linux/macOS POSIX workflow).

## Quickstart

### Cross-Platform / Windows (npm)

```sh
git clone <repo-url> rgb-node
cd rgb-node
npm run build      # compiles React web app and C++ firmware
npm run ota        # uploads firmware wirelessly over Wi-Fi
```

### Windows Helper Scripts

```cmd
:: Windows CMD
build.cmd

# Windows PowerShell
$env:PYTHONIOENCODING="utf-8"
.\build.ps1
```

### Linux / macOS (make)

```sh
make setup     # one-time: installs PlatformIO via uv
make build     # builds React web app & C++ firmware
make ota       # uploads firmware wirelessly over Wi-Fi
```

The first build downloads the ESP32 toolchain (a few hundred MB); later builds are fast.

## Features Overview

1. **Natural CCT Lighting**: Kelvin scale (2000K to 6500K) with Warmth percentage sliders.
2. **Decorative RGB**: Continuous HSL color wheel, quick swatches, Rainbow Cycle, Breathe, Candle Flicker, and Strobe.
3. **INMP441 Music Sync Engine**:
   - 🎵 Spectrum (RGB Frequency Mapping)
   - 🥁 Beat Pulse (Transient Bass Beat Trigger)
   - 🔊 Amplitude Modulation (Dynamic Volume Intensity)
   - 🌈 Pitch-to-Hue (Zero-Crossing Continuous Pitch Color Tracking)
   - 🌙 Ambient Chill (Low-Pass Ambient Ambience)
4. **Home Assistant MQTT Auto-Discovery**: Zero-touch integration via MQTT broadcast.
5. **OTA Wireless Updates**: Web drag-and-drop or CLI upload.

## Build Commands

| npm Script | Make Target | Action |
|---|---|---|
| `npm run build` | `make build` | Compile React web app + C++ firmware |
| `npm run build:web` | `make build-web` | Compile React web application (`web/dist`) |
| `npm run build:firmware` | — | Compile C++ firmware (`pio run`) |
| `npm run flash` | `make flash` | Build + upload over USB serial |
| `npm run ota` | `make ota` | Build + upload wirelessly over Wi-Fi (`esp32c3-supermini-ota`) |
| `npm run upload:fs` | `make upload-fs` | Upload LittleFS web filesystem image over USB serial |
| — | `make monitor` | Open the serial monitor at 115200 baud |
| — | `make clean` | Remove build artifacts |

## Troubleshooting

- **Windows Encoding Errors (`UnicodeDecodeError`):** PlatformIO on Windows requires UTF-8 output encoding. Ensure `PYTHONIOENCODING=utf-8` is set:
  - PowerShell: `$env:PYTHONIOENCODING="utf-8"`
  - CMD: `set PYTHONIOENCODING=utf-8`
- **`'make' is not recognized` on Windows:** Use `npm run build` or `build.cmd` instead of `make`.
- **`'pio' is not recognized`:** Add PlatformIO to PATH (`%USERPROFILE%\.local\bin` or `%USERPROFILE%\.platformio\penv\Scripts`), or install via `uv tool install platformio`.
- **No serial output:** The Super Mini uses the C3's native USB; `platformio.ini` already sets `ARDUINO_USB_CDC_ON_BOOT=1` to route `Serial` there — make sure you didn't remove it.

