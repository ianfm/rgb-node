#pragma once

#include <cstdint>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace config {

// RGB output pins. LEDC routes to any output GPIO, so these deliberately
// avoid the C3's few ADC1 inputs (GPIO 0–4), the strapping pins (8/9),
// and UART0 (20/21). GPIO5's ADC2 is unusable per chip erratum, so it's
// free for PWM. Change freely if your wiring differs.
constexpr uint8_t kPinRed = 5;
constexpr uint8_t kPinGreen = 6;
constexpr uint8_t kPinBlue = 7;

// Set -DRGBNODE_BRIGHTNESS_POT=1 in platformio.ini build_flags to enable pot.
// Default is 0 (70% fixed brightness, no pot).
#ifndef RGBNODE_BRIGHTNESS_POT
#define RGBNODE_BRIGHTNESS_POT 0
#endif

// Brightness potentiometer wiper (ends to 3V3 and GND). Must be an
// ADC1 pin — on the C3 that's GPIO 0–4 only; GPIO21 has no ADC (it's
// UART0 TX).
constexpr uint8_t kPinBrightness = 1;

constexpr uint32_t kPwmFreqHz = 5000;
constexpr uint8_t kPwmResolutionBits = 8;  // duty range 0–255
constexpr uint8_t kDefaultBrightness = 178; // 70% default brightness (178 / 255)

// Primary Wi-Fi Network
#ifdef SECRET_WIFI_SSID
#define WIFI_SSID SECRET_WIFI_SSID
#endif

#ifdef SECRET_WIFI_PASS
#define WIFI_PASS SECRET_WIFI_PASS
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "Your_WiFi_SSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "Your_WiFi_Password"
#endif

// Secondary Wi-Fi Network
#ifdef SECRET_WIFI_SSID2
#define WIFI_SSID2 SECRET_WIFI_SSID2
#endif

#ifdef SECRET_WIFI_PASS2
#define WIFI_PASS2 SECRET_WIFI_PASS2
#endif

#ifndef WIFI_SSID2
#define WIFI_SSID2 ""
#endif

#ifndef WIFI_PASS2
#define WIFI_PASS2 ""
#endif

constexpr char kWifiSsid[] = WIFI_SSID;
constexpr char kWifiPass[] = WIFI_PASS;
constexpr char kWifiSsid2[] = WIFI_SSID2;
constexpr char kWifiPass2[] = WIFI_PASS2;

constexpr char kHostname[] = "rgb-node";

// Soft AP fallback if all Station connections fail
constexpr char kApSsid[] = "RGBNode-Setup";
constexpr char kApPass[] = "12345678";

constexpr uint16_t kHttpPort = 80;

}  // namespace config
