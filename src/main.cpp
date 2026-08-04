// 3-channel PWM RGB LED driver for ESP32-C3 Super Mini
// Includes Web Server, WebSockets, NVS State Persistence, 12-Bit PWM, Gamma 2.8, Dynamic Effects, and MQTT Auto-Discovery.

#include <Arduino.h>
#include <Preferences.h>
#include <cmath>

#include "config.h"
#include "light_core.h"
#include "web_server.h"
#include "mqtt_provider.h"

static constexpr uint8_t kPinOnboardLed = 8;

static Preferences preferences;
static light_core::LightCore g_core;
static mqtt_provider::MqttProvider *g_mqtt = nullptr;
static web_server::LightState g_state;

// Render outputs in normalized float (0.0f to 1.0f) for smooth transitions
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
  preferences.putString("mode", g_state.mode);
  preferences.putUShort("colorTemp", g_state.colorTemp);
  preferences.putUChar("warmth", g_state.warmth);
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
  g_state.mode = preferences.getString("mode", "color");
  g_state.colorTemp = preferences.getUShort("colorTemp", 2700);
  g_state.warmth = preferences.getUChar("warmth", 84);
  g_state.effect = preferences.getString("effect", "static");
  g_state.speed = preferences.getUChar("speed", 50);
  preferences.end();
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
  ledcWrite(kPinOnboardLed, config::kPwmMaxDuty);

  loadStateFromNvs();
  g_core.init();

  mqtt_provider::MqttConfig mqttCfg;
  mqttCfg.host = config::kMqttHost;
  mqttCfg.port = config::kMqttPort;
  g_mqtt = new mqtt_provider::MqttProvider(&g_core, mqttCfg);
  g_mqtt->init();

  web_server::init(
      [](const web_server::LightState &newState) {
        g_state = newState;
        saveStateToNvs();
      },
      g_state);
}

void loop() {
  web_server::loop();
  if (g_mqtt) {
    g_mqtt->loop();
  }

  static uint32_t lastLoopTime = millis();
  const uint32_t now = millis();
  float deltaSec = (now - lastLoopTime) / 1000.0f;
  lastLoopTime = now;

  if (deltaSec < 0.001f) deltaSec = 0.001f;
  if (deltaSec > 0.1f) deltaSec = 0.1f;

  float targetR = 0.0f, targetG = 0.0f, targetB = 0.0f;
  g_core.getTargetRgb(targetR, targetG, targetB, deltaSec);

  const float brightMult = (g_state.power ? g_state.brightness : 0) / 255.0f;
  const float finalR = targetR * brightMult;
  const float finalG = targetG * brightMult;
  const float finalB = targetB * brightMult;

  g_currentR += (finalR - g_currentR) * 0.12f;
  g_currentG += (finalG - g_currentG) * 0.12f;
  g_currentB += (finalB - g_currentB) * 0.12f;

  const uint32_t dutyR = light_core::applyGamma12(g_currentR);
  const uint32_t dutyG = light_core::applyGamma12(g_currentG);
  const uint32_t dutyB = light_core::applyGamma12(g_currentB);
  const uint32_t dutyLed = config::kPwmMaxDuty - dutyR;

  ledcWrite(config::kPinRed, dutyR);
  ledcWrite(config::kPinGreen, dutyG);
  ledcWrite(config::kPinBlue, dutyB);
  ledcWrite(kPinOnboardLed, dutyLed);

  delay(10);
}
