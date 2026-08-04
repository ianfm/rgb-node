# AGENTS.md — Project Technical Guidelines

## 1. Hardware Architecture
- **MCU:** ESP32-C3 Super Mini (`board = esp32-c3-devkitm-1`).
- **PWM Pinout (`include/config.h`):**
  - `GPIO 5`: Red channel.
  - `GPIO 6`: Green channel.
  - `GPIO 7`: Blue channel.
  - `GPIO 8`: Onboard LED (active-low, mirrors Red).
  - *GPIO 0–4 are reserved for ADC1 inputs.*
- **PWM Settings:** `5000 Hz` frequency, `12-bit` resolution (`kPwmResolutionBits = 12`, range 0–4095).
- **Control Input:** Web/API controlled. Hardware potentiometer disabled (`RGBNODE_BRIGHTNESS_POT = 0`).
- **Agent Execution Rule:** For read-only/informational questions (e.g. pinouts, explanations), inspect codebase/history passively without running build, upload, or branch-modifying commands.

## 2. Firmware Specifications
- **State Persistence:** ESP32 `Preferences` (NVS) stores `power`, `r`, `g`, `b`, `brightness`, `effect`, `speed` across reboots.
- **Color Engine:** Continuous float hue mapping $h \in [0.0, 1.0)$ via `hueToRgbFloat()`.
- **Gamma Correction:** 12-bit power-law curve ($x^{2.8}$) via `applyGamma12()`.
- **Frame Timing:** 100 Hz (10ms) time-delta loop (`deltaSec`). Exponential smoothing filter (`alpha = 0.12f`) on target outputs.
- **OTA Updates:**
  - Dual app partitions (`app0`, `app1` at 1.375MB each) + LittleFS (1.1875MB) in `partitions.csv`.
  - Wireless CLI uploads via `ArduinoOTA` (`make ota` / `pio run -e esp32c3-supermini-ota -t upload`).
  - Web browser uploads via HTTP POST `/update` endpoint.

## 3. Web UI & Design System
- **Stack:** React 19 + Vite in `/web`, compiled to `/web/dist` and served via `LittleFS` on `ESPAsyncWebServer`.
- **Theme:** Clean Industrial / Technical Instrument (Matte Slate).
  - Background: `#0f172a` (slate-900)
  - Cards: `#1e293b` (slate-800)
  - Borders: 1px `#334155` (slate-700)
  - Text: `#f8fafc` / `#94a3b8` (slate-50 / slate-400)
  - Accent: `#3b82f6` (blue-500)
- **UI Constraints:** No drop-shadow glows, no neon accents, no gradients. Clean sans-serif (`Inter`) with monospace readouts (`JetBrains Mono`) for `#HEX`, `RGB()`, and `HSV()` numerical data.

## 4. Commands & Verification
- `make build-web` — Compile React app (`npm --prefix web run build`).
- `make build` — Compile C++ firmware (`pio run`).
- `make flash` — Upload firmware over USB (`pio run -e esp32c3-supermini -t upload`).
- `make upload-fs` — Upload LittleFS image over USB (`pio run -t uploadfs`).
- `make ota` — Upload firmware wirelessly over Wi-Fi (`pio run -e esp32c3-supermini-ota -t upload`).
- **Windows CLI Note:** Set `PYTHONIOENCODING=utf-8` on Windows for `pio` uploads (`cmd /c "set PYTHONIOENCODING=utf-8 && pio run -t upload"`).
