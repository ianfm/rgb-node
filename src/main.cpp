// 3-channel PWM RGB LED driver for ESP32-C3 Super Mini
// Includes Web Server, WebSockets, NVS State Persistence, and Dynamic Effects.

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "web_server.h"

static constexpr uint8_t kPinOnboardLed = 8;

static Preferences preferences;
static web_server::LightState g_state;

// Render outputs (for smooth power fading)
static float g_currentR = 0.0f;
static float g_currentG = 0.0f;
static float g_currentB = 0.0f;

// Save current state to NVS memory
static void saveStateToNvs() {
  preferences.begin("rgb_state", false);
  preferences.putBool("power", g_state.power);
  preferences.putUChar("r", g_state.r);
  preferences.putUChar("g", g_state.g);
  preferences.putUChar("b", g_state.b);
  preferences.putUChar("brightness", g_state.brightness);
  preferences.putString("effect", g_state.effect);
  preferences.putUChar("speed", g_state.speed);
  preferences.end();
}

// Load state from NVS memory
static void loadStateFromNvs() {
  preferences.begin("rgb_state", true);
  g_state.power = preferences.getBool("power", true);
  g_state.r = preferences.getUChar("r", 0);
  g_state.g = preferences.getUChar("g", 240);
  g_state.b = preferences.getUChar("b", 255);
  g_state.brightness = preferences.getUChar("brightness", 255);
  g_state.effect = preferences.getString("effect", "static");
  g_state.speed = preferences.getUChar("speed", 50);
  preferences.end();
}

// Color wheel conversion
static void hueToRgb(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t seg = h / 85;
  const uint8_t ramp = (h % 85) * 3;
  switch (seg) {
    case 0:  r = 255 - ramp; g = ramp;        b = 0;          break;
    case 1:  r = 0;          g = 255 - ramp;  b = ramp;       break;
    default: r = ramp;       g = 0;           b = 255 - ramp; break;
  }
}

void setup() {
  Serial.begin(115200);

  ledcAttach(config::kPinRed, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(config::kPinGreen, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(config::kPinBlue, config::kPwmFreqHz, config::kPwmResolutionBits);
  ledcAttach(kPinOnboardLed, config::kPwmFreqHz, config::kPwmResolutionBits);

  ledcWrite(config::kPinRed, 0);
  ledcWrite(config::kPinGreen, 0);
  ledcWrite(config::kPinBlue, 0);
  ledcWrite(kPinOnboardLed, 255);

  loadStateFromNvs();

  web_server::init(
      [](const web_server::LightState &newState) {
        g_state = newState;
        saveStateToNvs();
      },
      g_state);
}

void loop() {
  web_server::loop();

  static uint32_t lastAnimTime = 0;
  static uint8_t animHue = 0;
  static float breathePhase = 0.0f;
  static bool strobeState = false;

  uint8_t targetR = g_state.r;
  uint8_t targetG = g_state.g;
  uint8_t targetB = g_state.b;

  const uint32_t now = millis();
  const uint32_t speedMs = map(g_state.speed, 1, 100, 200, 10);

  if (g_state.power) {
    if (g_state.effect == "hue_cycle") {
      if (now - lastAnimTime >= speedMs) {
        lastAnimTime = now;
        animHue++;
      }
      hueToRgb(animHue, targetR, targetG, targetB);
    } else if (g_state.effect == "breathe") {
      if (now - lastAnimTime >= 16) {
        lastAnimTime = now;
        breathePhase += (0.02f * (g_state.speed / 50.0f));
        if (breathePhase > 6.28318f) breathePhase -= 6.28318f;
      }
      float mult = (sinf(breathePhase) + 1.0f) * 0.5f;
      targetR = (uint8_t)(targetR * mult);
      targetG = (uint8_t)(targetG * mult);
      targetB = (uint8_t)(targetB * mult);
    } else if (g_state.effect == "candle") {
      if (now - lastAnimTime >= speedMs) {
        lastAnimTime = now;
        float flick = random(70, 105) / 100.0f;
        targetR = (uint8_t)constrain(targetR * flick, 0.0f, 255.0f);
        targetG = (uint8_t)constrain(targetG * flick, 0.0f, 255.0f);
        targetB = (uint8_t)constrain(targetB * flick, 0.0f, 255.0f);
      }
    } else if (g_state.effect == "strobe") {
      if (now - lastAnimTime >= speedMs * 2) {
        lastAnimTime = now;
        strobeState = !strobeState;
      }
      if (!strobeState) {
        targetR = 0;
        targetG = 0;
        targetB = 0;
      }
    }
  } else {
    targetR = 0;
    targetG = 0;
    targetB = 0;
  }

  // Calculate master brightness factor
  const float brightMult = (g_state.power ? g_state.brightness : 0) / 255.0f;
  const float finalR = targetR * brightMult;
  const float finalG = targetG * brightMult;
  const float finalB = targetB * brightMult;

  // Smooth power transition easing
  g_currentR += (finalR - g_currentR) * 0.2f;
  g_currentG += (finalG - g_currentG) * 0.2f;
  g_currentB += (finalB - g_currentB) * 0.2f;

  const uint8_t rOut = (uint8_t)constrain(g_currentR, 0.0f, 255.0f);
  const uint8_t gOut = (uint8_t)constrain(g_currentG, 0.0f, 255.0f);
  const uint8_t bOut = (uint8_t)constrain(g_currentB, 0.0f, 255.0f);

  ledcWrite(config::kPinRed, rOut);
  ledcWrite(config::kPinGreen, gOut);
  ledcWrite(config::kPinBlue, bOut);
  ledcWrite(kPinOnboardLed, 255 - rOut);

  delay(10);
}
