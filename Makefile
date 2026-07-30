PIO := pio

.DEFAULT_GOAL := help

help:
	@echo "rgb-node — ESP32-C3 Super Mini RGB LED driver"
	@echo ""
	@echo "  make setup     install PlatformIO with uv (one-time)"
	@echo "  make build     compile the firmware"
	@echo "  make flash     build and upload over USB"
	@echo "  make monitor   open the serial monitor (Ctrl+C to exit)"
	@echo "  make clean     remove build artifacts"

setup:
	@command -v uv >/dev/null 2>&1 || { \
		echo "uv not found — install it first:"; \
		echo "  https://docs.astral.sh/uv/getting-started/installation/"; \
		exit 1; }
	uv tool install platformio

build:
	$(PIO) run

flash:
	$(PIO) run -t upload

monitor:
	$(PIO) device monitor

clean:
	$(PIO) run -t clean

.PHONY: help setup build flash monitor clean
