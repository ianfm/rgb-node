// Minimal 3-channel PWM test firmware: cycles a hue wheel across the
// R/G/B pins, with overall brightness set by a potentiometer. Replace
// loop() with real driver logic from here.

#include <Arduino.h>

#include "config.h"

// Onboard LED (active-low) mirrors the red channel — handy visual check
// that PWM and the brightness pot work without wiring anything up.
static constexpr uint8_t kPinOnboardLed = 8;

// Map a 0–255 hue onto the RGB channels (saturation/value fixed at max).
static void hueToRgb(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t seg = h / 85;        // thirds of the wheel
  const uint8_t ramp = (h % 85) * 3; // 0–252 within each third
  switch (seg) {
    case 0:  r = 255 - ramp; g = ramp;        b = 0;          break;
    case 1:  r = 0;          g = 255 - ramp;  b = ramp;       break;
    default: r = ramp;       g = 0;           b = 255 - ramp; break;
  }
}

#if RGBNODE_BRIGHTNESS_POT
// Pot reading as a 0–255 scale factor. EMA-smoothed so ADC noise doesn't
// flicker the LEDs; squared so the knob feels perceptually linear.
static uint8_t readBrightness() {
  static int32_t filtered = 0;
  filtered += (analogRead(config::kPinBrightness) - filtered) / 4;
  const uint32_t b = filtered >> 4;  // 12-bit ADC -> 0–255
  return (b * b) / 255;
}
#else
static uint8_t readBrightness() { return 255; }
#endif

void setup() {
  Serial.begin(115200);
#if RGBNODE_BRIGHTNESS_POT
  analogReadResolution(12);
#endif

  ledcAttach(config::kPinRed, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(config::kPinGreen, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(config::kPinBlue, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(kPinOnboardLed, config::kPwmFreqHz, config::kPwmResolutionBits);
}

void loop() {
  static uint8_t hue = 0;
  uint8_t r, g, b;
  hueToRgb(hue, r, g, b);

  const uint16_t bright = readBrightness();
  const uint8_t rOut = r * bright / 255;
  ledcWrite(config::kPinRed, rOut);
  ledcWrite(config::kPinGreen, g * bright / 255);
  ledcWrite(config::kPinBlue, b * bright / 255);
  ledcWrite(kPinOnboardLed, 255 - rOut);  // active-low

  // Heartbeat for `make monitor`, once per wheel (~5 s).
  if (++hue == 0) {
    Serial.printf("hue wheel complete, brightness %u%%\n", bright * 100 / 255);
  }
  delay(20);
}
