# ESP32-C3 Super Mini — Project Template

Reusable configuration for any new **ESP32-C3 Super Mini** project using PlatformIO and the Arduino 3.x core (`pioarduino`).

## 1. `platformio.ini`

Copy this into your new project's `platformio.ini`:

```ini
[env:esp32c3-supermini]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
build_flags =
    ; Required for Super Mini native USB serial monitoring:
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

> **Note**: Because the platform toolchain is now installed in `~/.platformio`, any new project on this computer using this exact `platform` line will build **instantly** without re-downloading toolchains or triggering setup timeouts.

---

## 2. `Makefile`

Copy this into your new project's `Makefile`:

```makefile
PIO := pio

.DEFAULT_GOAL := help

help:
	@echo "ESP32-C3 Super Mini Project Commands:"
	@echo "  make build     compile firmware"
	@echo "  make flash     build and upload over USB"
	@echo "  make release   build and copy binaries to releases/v0.01/"
	@echo "  make monitor   open serial monitor (Ctrl+C to exit)"
	@echo "  make clean     remove build artifacts"

build:
	$(PIO) run

flash:
	$(PIO) run -t upload

release: build
	mkdir -p releases/v0.01
	cp .pio/build/esp32c3-supermini/firmware.factory.bin releases/v0.01/
	cp .pio/build/esp32c3-supermini/firmware.bin releases/v0.01/
	cp .pio/build/esp32c3-supermini/bootloader.bin releases/v0.01/
	cp .pio/build/esp32c3-supermini/partitions.bin releases/v0.01/
	@echo "Release binaries saved to releases/v0.01/"

monitor:
	$(PIO) device monitor

clean:
	$(PIO) run -t clean

.PHONY: help build flash release monitor clean
```
