#pragma once

#include <cstdint>

namespace config {

// RGB output pins. LEDC routes to any output GPIO, so these deliberately
// avoid the C3's few ADC1 inputs (GPIO 0–4), the strapping pins (8/9),
// and UART0 (20/21). GPIO5's ADC2 is unusable per chip erratum, so it's
// free for PWM. Change freely if your wiring differs.
constexpr uint8_t kPinRed = 5;
constexpr uint8_t kPinGreen = 6;
constexpr uint8_t kPinBlue = 7;

// Brightness potentiometer wiper (ends to 3V3 and GND). Must be an
// ADC1 pin — on the C3 that's GPIO 0–4 only; GPIO21 has no ADC (it's
// UART0 TX).
constexpr uint8_t kPinBrightness = 1;

constexpr uint32_t kPwmFreqHz = 5000;
constexpr uint8_t kPwmResolutionBits = 8;  // duty range 0–255

}  // namespace config
