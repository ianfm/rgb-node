// Includes Web Server, WebSockets, NVS State Persistence, 12-Bit PWM, Gamma 2.8, Dynamic Effects, INMP441 Music Sync, and Home Assistant MQTT.

#include <Arduino.h>
#include <Preferences.h>
#include <cmath>

#include "audio_dsp.h"
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

static bool g_nvsDirty = false;
static uint32_t g_lastStateChangeMs = 0;
static web_server::LightState g_lastSavedState;
static bool g_hasSavedState = false;

static void markStateDirty() {
  g_nvsDirty = true;
  g_lastStateChangeMs = millis();
}

// Save current state to NVS memory (delta/diff only to preserve flash & avoid stalls)
static void saveStateToNvs() {
  preferences.begin("rgb_state", false);
  if (!g_hasSavedState || g_state.power != g_lastSavedState.power) preferences.putBool("power", g_state.power);
  if (!g_hasSavedState || g_state.r != g_lastSavedState.r) preferences.putUChar("r", g_state.r);
  if (!g_hasSavedState || g_state.g != g_lastSavedState.g) preferences.putUChar("g", g_state.g);
  if (!g_hasSavedState || g_state.b != g_lastSavedState.b) preferences.putUChar("b", g_state.b);
  if (!g_hasSavedState || g_state.brightness != g_lastSavedState.brightness) preferences.putUChar("brightness", g_state.brightness);
  if (!g_hasSavedState || g_state.mode != g_lastSavedState.mode) preferences.putString("mode", g_state.mode);
  if (!g_hasSavedState || g_state.colorTemp != g_lastSavedState.colorTemp) preferences.putUShort("colorTemp", g_state.colorTemp);
  if (!g_hasSavedState || g_state.warmth != g_lastSavedState.warmth) preferences.putUChar("warmth", g_state.warmth);
  if (!g_hasSavedState || g_state.effect != g_lastSavedState.effect) preferences.putString("effect", g_state.effect);
  if (!g_hasSavedState || g_state.speed != g_lastSavedState.speed) preferences.putUChar("speed", g_state.speed);
  if (!g_hasSavedState || g_state.musicSensitivity != g_lastSavedState.musicSensitivity) preferences.putUChar("musicSens", g_state.musicSensitivity);
  if (!g_hasSavedState || g_state.noiseCutoff != g_lastSavedState.noiseCutoff) preferences.putUChar("noiseCut", g_state.noiseCutoff);
  if (!g_hasSavedState || g_state.headroom != g_lastSavedState.headroom) preferences.putUChar("headroom", g_state.headroom);
  if (!g_hasSavedState || g_state.responseAgility != g_lastSavedState.responseAgility) preferences.putUChar("agility", g_state.responseAgility);
  if (!g_hasSavedState || g_state.beatSens != g_lastSavedState.beatSens) preferences.putUChar("beatSens", g_state.beatSens);
  if (!g_hasSavedState || g_state.beatDecay != g_lastSavedState.beatDecay) preferences.putUShort("beatDecay", g_state.beatDecay);
  if (!g_hasSavedState || g_state.pitchLowHz != g_lastSavedState.pitchLowHz) preferences.putUShort("pitchLow", g_state.pitchLowHz);
  if (!g_hasSavedState || g_state.pitchHighHz != g_lastSavedState.pitchHighHz) preferences.putUShort("pitchHigh", g_state.pitchHighHz);
  if (!g_hasSavedState || g_state.pitchSmooth != g_lastSavedState.pitchSmooth) preferences.putUChar("pitchSmooth", g_state.pitchSmooth);
  if (!g_hasSavedState || g_state.ambientGlow != g_lastSavedState.ambientGlow) preferences.putUChar("ambientGlow", g_state.ambientGlow);
  if (!g_hasSavedState || g_state.useLogScale != g_lastSavedState.useLogScale) preferences.putBool("useLogScale", g_state.useLogScale);
  preferences.end();

  g_lastSavedState = g_state;
  g_hasSavedState = true;
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
  g_state.musicSensitivity = preferences.getUChar("musicSens", 50);
  g_state.noiseCutoff = preferences.getUChar("noiseCut", 8);
  g_state.headroom = preferences.getUChar("headroom", 150);
  g_state.responseAgility = preferences.getUChar("agility", 50);
  g_state.beatSens = preferences.getUChar("beatSens", 45);
  g_state.beatDecay = preferences.getUShort("beatDecay", 180);
  g_state.pitchLowHz = preferences.getUShort("pitchLow", 120);
  g_state.pitchHighHz = preferences.getUShort("pitchHigh", 2400);
  g_state.pitchSmooth = preferences.getUChar("pitchSmooth", 8);
  g_state.ambientGlow = preferences.getUChar("ambientGlow", 0);
  g_state.useLogScale = preferences.getBool("useLogScale", true);
  preferences.end();

  g_lastSavedState = g_state;
  g_hasSavedState = true;
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

  audio_dsp::init(config::kPinI2sBclk, config::kPinI2sWs, config::kPinI2sDin);

  web_server::init(
      [](const web_server::LightState &newState) {
        g_state = newState;
        markStateDirty();
      },
      g_state);
}

void loop() {
  web_server::loop();
  if (g_mqtt) {
    g_mqtt->loop();
  }

  // Debounced NVS state persistence (saves 3s after the last state modification stops)
  if (g_nvsDirty && (millis() - g_lastStateChangeMs >= 3000)) {
    g_nvsDirty = false;
    saveStateToNvs();
  }

  static uint32_t lastLoopTime = millis();
  const uint32_t now = millis();
  float deltaSec = (now - lastLoopTime) / 1000.0f;
  lastLoopTime = now;

  if (deltaSec < 0.001f) deltaSec = 0.001f;
  if (deltaSec > 0.1f) deltaSec = 0.1f;

  static float animHue = 0.0f;
  static float breathePhase = 0.0f;
  static bool strobeState = false;
  static float strobeTimer = 0.0f;
  static float pulseDecay = 0.0f;

  // Pass active DSP Configuration to INMP441 audio engine
  audio_dsp::DspConfig dspCfg;
  dspCfg.sensitivity = g_state.musicSensitivity;
  dspCfg.noiseCutoff = g_state.noiseCutoff;
  dspCfg.headroom = g_state.headroom;
  dspCfg.beatSens = g_state.beatSens;
  dspCfg.pitchLowHz = g_state.pitchLowHz;
  dspCfg.pitchHighHz = g_state.pitchHighHz;
  dspCfg.pitchSmooth = g_state.pitchSmooth;
  audio_dsp::setConfig(dspCfg);

  audio_dsp::AudioBands bands = audio_dsp::getBands();

  float targetR = g_state.r / 255.0f;
  float targetG = g_state.g / 255.0f;
  float targetB = g_state.b / 255.0f;

  float volAmp = g_state.useLogScale ? bands.logAmp : bands.totalAmp;
  float ambGlow = (float)g_state.ambientGlow / 100.0f;

  if (g_state.power) {
    if (g_state.mode == "white") {
      light_core::kelvinToRgbFloat(g_state.colorTemp, targetR, targetG, targetB);
    } else if (g_state.effect == "hue_cycle") {
      const float cycleFreqHz = 0.02f + (g_state.speed - 1) * (0.48f / 99.0f);
      animHue += deltaSec * cycleFreqHz;
      if (animHue >= 1.0f) animHue -= 1.0f;
      light_core::hueToRgbFloat(animHue, targetR, targetG, targetB);
    } else if (g_state.effect == "breathe") {
      const float breatheFreqHz = 0.1f + (g_state.speed - 1) * (1.9f / 99.0f);
      breathePhase += deltaSec * breatheFreqHz * 6.2831853f;
      if (breathePhase >= 6.2831853f) breathePhase -= 6.2831853f;
      const float mult = (sinf(breathePhase) + 1.0f) * 0.5f;
      targetR *= mult;
      targetG *= mult;
      targetB *= mult;
    } else if (g_state.effect == "candle") {
      static float candleFlicker = 1.0f;
      static float candleTarget = 1.0f;
      static float candleTimer = 0.0f;
      candleTimer += deltaSec;
      const float changeInterval = 0.08f + (100 - g_state.speed) * (0.25f / 99.0f);
      if (candleTimer >= changeInterval) {
        candleTimer = 0.0f;
        candleTarget = random(60, 100) / 100.0f;
      }
      candleFlicker += (candleTarget - candleFlicker) * 0.15f;
      targetR *= candleFlicker;
      targetG *= candleFlicker;
      targetB *= candleFlicker;
    } else if (g_state.effect == "strobe") {
      const float strobeInterval = 0.3f - (g_state.speed - 1) * (0.28f / 99.0f);
      strobeTimer += deltaSec;
      if (strobeTimer >= strobeInterval) {
        strobeTimer = 0.0f;
        strobeState = !strobeState;
      }
      if (!strobeState) {
        targetR = 0.0f;
        targetG = 0.0f;
        targetB = 0.0f;
      }
    } else if (g_state.effect == "music_spectrum") {
      targetR = bands.bass;
      targetG = bands.mid;
      targetB = bands.treble;
    } else if (g_state.effect == "music_pulse") {
      const float cycleFreqHz = 0.02f + (g_state.speed - 1) * (0.48f / 99.0f);
      animHue += deltaSec * cycleFreqHz;
      if (animHue >= 1.0f) animHue -= 1.0f;
      light_core::hueToRgbFloat(animHue, targetR, targetG, targetB);

      float decayFactor = expf(-deltaSec * 1000.0f / (float)fmaxf(10.0f, (float)g_state.beatDecay));
      if (bands.beat) {
        pulseDecay = 1.0f;
      } else {
        pulseDecay = fmaxf(ambGlow, pulseDecay * decayFactor);
      }
      targetR *= pulseDecay;
      targetG *= pulseDecay;
      targetB *= pulseDecay;
    } else if (g_state.effect == "music_amplitude") {
      float dynamicScale = ambGlow + (1.0f - ambGlow) * volAmp;
      targetR *= dynamicScale;
      targetG *= dynamicScale;
      targetB *= dynamicScale;
    } else if (g_state.effect == "music_freq_hue") {
      light_core::hueToRgbFloat(bands.dominantHue, targetR, targetG, targetB);
      float dynamicScale = ambGlow + (1.0f - ambGlow) * volAmp;
      targetR *= dynamicScale;
      targetG *= dynamicScale;
      targetB *= dynamicScale;
    } else if (g_state.effect == "music_chill") {
      float spectR = bands.bass;
      float spectG = bands.mid;
      float spectB = bands.treble;
      targetR = targetR * 0.4f + spectR * 0.6f;
      targetG = targetG * 0.4f + spectG * 0.6f;
      targetB = targetB * 0.4f + spectB * 0.6f;
      float dynamicScale = ambGlow + (1.0f - ambGlow) * volAmp;
      targetR *= dynamicScale;
      targetG *= dynamicScale;
      targetB *= dynamicScale;
    }
  } else {
    targetR = 0.0f;
    targetG = 0.0f;
    targetB = 0.0f;
  }

  // Master Brightness Ceiling (Upper Cap)
  const float brightMult = (g_state.power ? g_state.brightness : 0) / 255.0f;
  const float finalR = targetR * brightMult;
  const float finalG = targetG * brightMult;
  const float finalB = targetB * brightMult;

  // Dynamic Slew Rate Limiter (Configurable Response Agility 1..100)
  float alpha = 0.04f + ((float)g_state.responseAgility / 100.0f) * 0.36f; // 0.04 (ultra smooth) to 0.40 (fast responsive)
  g_currentR += (finalR - g_currentR) * alpha;
  g_currentG += (finalG - g_currentG) * alpha;
  g_currentB += (finalB - g_currentB) * alpha;

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
