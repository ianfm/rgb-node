// 3-channel PWM RGB LED driver for ESP32-C3 Super Mini
// Includes Web Server, WebSockets, NVS State Persistence, 12-Bit PWM, Gamma 2.8, Dynamic Effects, and INMP441 I2S Music Sync.

#include <Arduino.h>
#include <Preferences.h>
#include <cmath>

#include "audio_dsp.h"
#include "config.h"
#include "web_server.h"

static constexpr uint8_t kPinOnboardLed = 8;

static Preferences preferences;
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
  preferences.putString("effect", g_state.effect);
  preferences.putUChar("speed", g_state.speed);
  preferences.putUChar("musicSens", g_state.musicSensitivity);
  preferences.putUChar("noiseCut", g_state.noiseCutoff);
  preferences.putUChar("beatSens", g_state.beatSens);
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
  g_state.musicSensitivity = preferences.getUChar("musicSens", 50);
  g_state.noiseCutoff = preferences.getUChar("noiseCut", 8);
  g_state.beatSens = preferences.getUChar("beatSens", 45);
  preferences.end();
}

// Continuous 6-segment floating-point color wheel (hueNorm in 0.0f..1.0f)
static void hueToRgbFloat(float hueNorm, float &r, float &g, float &b) {
  hueNorm = fmodf(hueNorm, 1.0f);
  if (hueNorm < 0.0f) hueNorm += 1.0f;

  float h6 = hueNorm * 6.0f;
  int i = (int)h6;
  float f = h6 - i;
  float q = 1.0f - f;
  float t = f;

  switch (i % 6) {
    case 0:  r = 1.0f; g = t;    b = 0.0f; break;
    case 1:  r = q;    g = 1.0f; b = 0.0f; break;
    case 2:  r = 0.0f; g = 1.0f; b = t;    break;
    case 3:  r = 0.0f; g = q;    b = 1.0f; break;
    case 4:  r = t;    g = 0.0f; b = 1.0f; break;
    default: r = 1.0f; g = 0.0f; b = q;    break;
  }
}

// 12-bit Gamma 2.8 perceptual brightness correction
static uint32_t applyGamma12(float normalized) {
  normalized = constrain(normalized, 0.0f, 1.0f);
  if (normalized <= 0.0001f) return 0;
  float gammaCorrected = powf(normalized, 2.8f);
  return (uint32_t)roundf(gammaCorrected * config::kPwmMaxDuty);
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

  audio_dsp::init(config::kPinI2sBclk, config::kPinI2sWs, config::kPinI2sDin);

  web_server::init(
      [](const web_server::LightState &newState) {
        g_state = newState;
        saveStateToNvs();
      },
      g_state);
}

void loop() {
  web_server::loop();

  static uint32_t lastLoopTime = millis();
  const uint32_t now = millis();
  float deltaSec = (now - lastLoopTime) / 1000.0f;
  lastLoopTime = now;

  // Clamp deltaSec to prevent massive jumps on boot or long stalls
  if (deltaSec < 0.001f) deltaSec = 0.001f;
  if (deltaSec > 0.1f) deltaSec = 0.1f;

  static float animHue = 0.0f;
  static float breathePhase = 0.0f;
  static bool strobeState = false;
  static float strobeTimer = 0.0f;
  static float pulseDecay = 0.0f;

  audio_dsp::setSensitivity(g_state.musicSensitivity);
  audio_dsp::AudioBands bands = audio_dsp::getBands();

  float targetR = g_state.r / 255.0f;
  float targetG = g_state.g / 255.0f;
  float targetB = g_state.b / 255.0f;

  if (g_state.power) {
    if (g_state.effect == "hue_cycle") {
      const float cycleFreqHz = 0.02f + (g_state.speed - 1) * (0.48f / 99.0f);
      animHue += deltaSec * cycleFreqHz;
      if (animHue >= 1.0f) animHue -= 1.0f;
      hueToRgbFloat(animHue, targetR, targetG, targetB);
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
      hueToRgbFloat(animHue, targetR, targetG, targetB);

      if (bands.beat) {
        pulseDecay = 1.0f;
      } else {
        pulseDecay = fmaxf(0.05f, pulseDecay * 0.88f);
      }
      targetR *= pulseDecay;
      targetG *= pulseDecay;
      targetB *= pulseDecay;
    } else if (g_state.effect == "music_amplitude") {
      const float cutoff = g_state.noiseCutoff / 100.0f;
      float cleanAmp = (bands.totalAmp < cutoff) ? 0.0f : constrain((bands.totalAmp - cutoff) / (1.0f - cutoff), 0.0f, 1.0f);
      targetR *= cleanAmp;
      targetG *= cleanAmp;
      targetB *= cleanAmp;
    } else if (g_state.effect == "music_freq_hue") {
      const float cutoff = g_state.noiseCutoff / 100.0f;
      float cleanAmp = (bands.totalAmp < cutoff) ? 0.0f : constrain((bands.totalAmp - cutoff) / (1.0f - cutoff), 0.0f, 1.0f);
      hueToRgbFloat(bands.dominantHue, targetR, targetG, targetB);
      targetR *= cleanAmp;
      targetG *= cleanAmp;
      targetB *= cleanAmp;
    } else if (g_state.effect == "music_chill") {
      const float cutoff = g_state.noiseCutoff / 100.0f;
      float cleanAmp = (bands.totalAmp < cutoff) ? 0.0f : constrain((bands.totalAmp - cutoff) / (1.0f - cutoff), 0.0f, 1.0f);
      float spectR = bands.bass;
      float spectG = bands.mid;
      float spectB = bands.treble;
      targetR = targetR * 0.4f + spectR * 0.6f;
      targetG = targetG * 0.4f + spectG * 0.6f;
      targetB = targetB * 0.4f + spectB * 0.6f;
      targetR *= (0.2f + 0.8f * cleanAmp);
      targetG *= (0.2f + 0.8f * cleanAmp);
      targetB *= (0.2f + 0.8f * cleanAmp);
    }
  } else {
    targetR = 0.0f;
    targetG = 0.0f;
    targetB = 0.0f;
  }

  // Calculate master brightness factor
  const float brightMult = (g_state.power ? g_state.brightness : 0) / 255.0f;
  const float finalR = targetR * brightMult;
  const float finalG = targetG * brightMult;
  const float finalB = targetB * brightMult;

  // Sub-step low-pass exponential smoothing at 100 Hz
  g_currentR += (finalR - g_currentR) * 0.12f;
  g_currentG += (finalG - g_currentG) * 0.12f;
  g_currentB += (finalB - g_currentB) * 0.12f;

  // Apply 12-bit Gamma 2.8 curve
  const uint32_t dutyR = applyGamma12(g_currentR);
  const uint32_t dutyG = applyGamma12(g_currentG);
  const uint32_t dutyB = applyGamma12(g_currentB);
  const uint32_t dutyLed = config::kPwmMaxDuty - dutyR;  // active low onboard LED

  ledcWrite(config::kPinRed, dutyR);
  ledcWrite(config::kPinGreen, dutyG);
  ledcWrite(config::kPinBlue, dutyB);
  ledcWrite(kPinOnboardLed, dutyLed);

  delay(10);
}
