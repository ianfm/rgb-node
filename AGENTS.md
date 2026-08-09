# AGENTS.md — Project Technical Guidelines

## 1. Hardware Architecture & Top-Level Pinout
- **MCU:** ESP32-C3 Super Mini (`board = esp32-c3-devkitm-1`).
- **Complete Pinout Map (`include/config.h`):**

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

- **PWM Settings:** `5000 Hz` frequency, `12-bit` resolution (`kPwmResolutionBits = 12`, range 0–4095).
- **Control Input:** Web/API controlled. Hardware potentiometer disabled (`RGBNODE_BRIGHTNESS_POT = 0`).
- **Agent Execution Rule:** For read-only/informational questions (e.g. pinouts, explanations), inspect codebase/history passively without running build, upload, or branch-modifying commands.

## 2. Firmware Specifications
- **State Persistence:** ESP32 `Preferences` (NVS) stores all lighting, CCT, and audio DSP parameters across reboots.
- **Color Engine:** Continuous float hue mapping $h \in [0.0, 1.0)$ via `hueToRgbFloat()`.
- **Gamma Correction:** 12-bit power-law curve ($x^{2.8}$) via `applyGamma12()`.
- **Frame Timing:** 100 Hz (10ms) time-delta loop (`deltaSec`). Dynamic rate-limiting filter (`responseAgility`) on target outputs.
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

### Cross-Platform & Windows Native Commands (Primary)
- `npm run build` — Compile both React Web UI and C++ firmware (`npm --prefix web run build` + `pio run`).
- `npm run build:web` — Compile React web app (`npm --prefix web run build`).
- `npm run build:firmware` — Compile C++ firmware (`pio run -e esp32c3-supermini -e esp32c3-supermini-ota`).
- `npm run flash` — Upload firmware over USB (`pio run -e esp32c3-supermini -t upload`).
- `npm run upload:fs` / `npm run upload-fs` — Upload LittleFS static web assets over USB (`pio run -t uploadfs`).
- `npm run ota` — Upload firmware wirelessly over Wi-Fi (`pio run -e esp32c3-supermini-ota -t upload`).
- `npm run ota:fs` / `npm run ota-fs` — Upload LittleFS static web assets over Wi-Fi (`pio run -e esp32c3-supermini-ota -t uploadfs`).
- `npm run ota:all` / `npm run ota-all` — Upload both C++ firmware and LittleFS web assets over Wi-Fi (`npm run ota && npm run ota:fs`).

### GNU Make Commands (Optional / POSIX)
- `make build-web` — Alias for `npm --prefix web run build`.
- `make build` — Alias for `pio run -e esp32c3-supermini -e esp32c3-supermini-ota`.
- `make flash` — Alias for USB flash upload.
- `make upload-fs` — Upload LittleFS static web assets over USB.
- `make ota` — Upload C++ firmware wirelessly over Wi-Fi.
- `make ota-fs` — Upload LittleFS static web assets wirelessly over Wi-Fi.
- `make ota-all` — Upload both C++ firmware and LittleFS static web assets wirelessly over Wi-Fi.

### Windows Agent Execution Guidelines
- **Windows CLI Encoding:** Always ensure `PYTHONIOENCODING=utf-8` is set when running `pio` commands in Windows CMD or PowerShell (e.g. `cmd /c "set PYTHONIOENCODING=utf-8 && pio run"` or `$env:PYTHONIOENCODING="utf-8"` in PowerShell).
- **Missing `make` on Windows:** Standard Windows CMD/PowerShell environments do not include GNU `make`. AI agents operating on Windows MUST use `npm run <script>` or direct `pio` / `npm` commands instead of assuming `make` is installed.
- **PlatformIO Binary Path Resolution:** If `pio` is not found on Windows PATH, check Python/uv script locations:
  - `%USERPROFILE%\.local\bin\pio.exe`
  - `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`
  - Or invoke via `uv tool run platformio run` / `python -m platformio run`.

