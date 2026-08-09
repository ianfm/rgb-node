PIO := pio
#DEV = --upload-port /dev/ttyACM2

.DEFAULT_GOAL := help

help:
	@echo "rgb-node — ESP32-C3 Super Mini RGB LED driver"
	@echo ""
	@echo "  make setup      install PlatformIO with uv (one-time)"
	@echo "  make build-web  compile the React web application"
	@echo "  make build      compile the firmware"
	@echo "  make flash      build and upload firmware over USB"
	@echo "  make ota        flash firmware wirelessly over Wi-Fi (OTA)"
	@echo "  make flash-ota  alias for make ota"
	@echo "  make ota-fs     upload LittleFS static web UI wirelessly over Wi-Fi (OTA)"
	@echo "  make ota-all    upload both firmware and LittleFS web UI wirelessly (OTA)"
	@echo "  make upload-fs  upload LittleFS static web UI over USB"
	@echo "  make release    build and package binaries to releases/v0.01/"
	@echo "  make monitor    open the serial monitor (Ctrl+C to exit)"
	@echo "  make clean      remove build artifacts"

setup:
	@command -v uv >/dev/null 2>&1 || { \
		echo "uv not found — install it first:"; \
		echo "  https://docs.astral.sh/uv/getting-started/installation/"; \
		exit 1; }
	uv tool install platformio

build-web:
	cd web && npm install && npm run build

build: build-web
	$(PIO) run -e esp32c3-supermini -e esp32c3-supermini-ota


flash:
	$(PIO) run -e esp32c3-supermini -t upload

ota:
	$(PIO) run -e esp32c3-supermini-ota -t upload

flash-ota: ota

ota-fs: build-web
	$(PIO) run -e esp32c3-supermini-ota -t uploadfs

ota-all: ota ota-fs

upload-fs: build-web
	$(PIO) run -t uploadfs

release: build build-web
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

.PHONY: help setup build-web build flash ota flash-ota ota-fs ota-all upload-fs release monitor clean
