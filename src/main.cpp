// 3-channel PWM RGB LED driver for the ESP32-C3 Super Mini with Web Server & Serial control.

#include <Arduino.h>

#include "config.h"
#include "web_server.h"

// Onboard LED (active-low) mirrors the red channel — handy visual check
static constexpr uint8_t kPinOnboardLed = 8;

static uint8_t g_targetR = 0;
static uint8_t g_targetG = 0;
static uint8_t g_targetB = 0;
static bool g_webControlReceived = false;

// Map a 0–255 hue onto the RGB channels (saturation/value fixed at max).
static void hueToRgb(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t seg = h / 85;         // thirds of the wheel
  const uint8_t ramp = (h % 85) * 3;  // 0–252 within each third
  switch (seg) {
    case 0:
      r = 255 - ramp;
      g = ramp;
      b = 0;
      break;
    case 1:
      r = 0;
      g = 255 - ramp;
      b = ramp;
      break;
    default:
      r = ramp;
      g = 0;
      b = 255 - ramp;
      break;
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
static uint8_t readBrightness() { return config::kDefaultBrightness; } // 70% default brightness
#endif

// Check for incoming serial commands: "<stripIndex>,<R>,<G>,<B>\n"
static void handleSerialCommands() {
  if (Serial.available() > 0) {
    int stripIdx = Serial.parseInt();
    int r = Serial.parseInt();
    int g = Serial.parseInt();
    int b = Serial.parseInt();

    while (Serial.available() && (Serial.peek() == '\n' || Serial.peek() == '\r' ||
                                 Serial.peek() == ' ')) {
      Serial.read();
    }

    if (stripIdx == 0) {
      g_targetR = (uint8_t)constrain(r, 0, 255);
      g_targetG = (uint8_t)constrain(g, 0, 255);
      g_targetB = (uint8_t)constrain(b, 0, 255);
      g_webControlReceived = true;
      web_server::broadcastState(g_targetR, g_targetG, g_targetB);
    }
  }
}

void setup() {
  Serial.begin(115200);
#if RGBNODE_BRIGHTNESS_POT
  analogReadResolution(12);
#endif

  ledcAttach(config::kPinRed, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(config::kPinGreen, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(config::kPinBlue, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(kPinOnboardLed, config::kPwmFreqHz, config::kPwmResolutionBits);

  // Ensure all channels start completely OFF (0V)
  ledcWrite(config::kPinRed, 0);
  ledcWrite(config::kPinGreen, 0);
  ledcWrite(config::kPinBlue, 0);
  ledcWrite(kPinOnboardLed, 255);  // active-low onboard LED (off)

  // Initialize Web Server, WebSockets, and Wi-Fi
  web_server::init([](uint8_t r, uint8_t g, uint8_t b) {
    g_targetR = r;
    g_targetG = g;
    g_targetB = b;
    g_webControlReceived = true;
  });
}

void loop() {
  web_server::loop();
  handleSerialCommands();

  // If no external web/serial control has been received yet, run demo hue wheel
  if (!g_webControlReceived) {
    static uint8_t hue = 0;
    static uint32_t lastHueUpdate = 0;
    if (millis() - lastHueUpdate > 20) {
      lastHueUpdate = millis();
      hueToRgb(hue++, g_targetR, g_targetG, g_targetB);
    }
  }

  // Apply potentiometer overall brightness scaling
  const uint16_t bright = readBrightness();
  const uint8_t rOut = (uint16_t)g_targetR * bright / 255;
  const uint8_t gOut = (uint16_t)g_targetG * bright / 255;
  const uint8_t bOut = (uint16_t)g_targetB * bright / 255;

  ledcWrite(config::kPinRed, rOut);
  ledcWrite(config::kPinGreen, gOut);
  ledcWrite(config::kPinBlue, bOut);
  ledcWrite(kPinOnboardLed, 255 - rOut);  // active-low onboard LED

  delay(5);
}
