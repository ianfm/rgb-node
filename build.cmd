@echo off
setlocal enabledelayedexpansion
set PYTHONIOENCODING=utf-8

echo [rgb-node] Building React Web UI...
call npm --prefix web run build
if %errorlevel% neq 0 (
    echo [rgb-node] ERROR: Web UI build failed.
    exit /b %errorlevel%
)

echo [rgb-node] Building C++ Firmware...
call pio run -e esp32c3-supermini -e esp32c3-supermini-ota
if %errorlevel% neq 0 (
    echo [rgb-node] ERROR: Firmware build failed.
    exit /b %errorlevel%
)

echo [rgb-node] Build completed successfully.
