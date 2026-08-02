# rgb-node

3-channel PWM RGB LED driver firmware for the ESP32-C3 Super Mini.

## Prerequisites

- [uv](https://docs.astral.sh/uv/getting-started/installation/) — used to install PlatformIO; PlatformIO then fetches the pinned toolchain and framework automatically on first build.
- `make` and `git`.

## Quickstart

```sh
git clone <repo-url> rgb-node
cd rgb-node
make setup     # one-time: installs PlatformIO via uv
make flash     # builds and uploads over USB
make monitor   # watch serial output (Ctrl+C to exit)
```

The first build downloads the ESP32 toolchain (a few hundred MB); later builds are fast.

The stock firmware slowly cycles the hue across the R/G/B pins (GPIO 5/6/7 by default — see `include/config.h`) so you can immediately verify the board flashed and the outputs work. Those pins were picked to leave the C3's only ADC inputs (GPIO 0–4) free for analog use. The onboard LED (GPIO8) mirrors the red channel as a no-wiring sanity check.

## Brightness pot

Overall brightness is set by a potentiometer: outer legs to 3V3 and GND, wiper to **GPIO1**. Any value from ~10 kΩ up is fine. With nothing connected the pin floats and brightness will be arbitrary. The pin must be one of GPIO 0–4 — those are the only usable ADC inputs on the ESP32-C3 (GPIO5's ADC2 is broken by chip erratum, and pins like 20/21 are UART with no ADC at all).

No pot wired up? Build without it with `-DRGBNODE_BRIGHTNESS_POT=0` in `platformio.ini`'s `build_flags` — brightness is fixed at 70% by default and the ADC is left untouched.

## Make targets

| Target | Does |
|---|---|
| `make setup` | Install PlatformIO with `uv tool install platformio` |
| `make build` | Compile the firmware |
| `make flash` | Build + upload (serial port auto-detected) |
| `make monitor` | Open the serial monitor at 115200 baud |
| `make clean` | Remove build artifacts |

## Troubleshooting

- **Board doesn't show up as a serial port:** hold the BOOT button while plugging in the USB cable to force download mode, then `make flash` again.
- **Permission denied on `/dev/ttyACM0` (Linux):** add yourself to the serial group and re-login: `sudo usermod -aG dialout $USER` (some distros use `uucp` instead).
- **No serial output:** the Super Mini uses the C3's native USB; `platformio.ini` already sets `ARDUINO_USB_CDC_ON_BOOT=1` to route `Serial` there — make sure you didn't remove it.

## Hardware note

The C3's GPIOs can only source a few mA — enough for an indicator LED with a resistor, but not LED strips or power LEDs. Drive each channel through a logic-level MOSFET (or an LED driver stage) for real loads.
