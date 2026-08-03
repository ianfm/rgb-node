#include "light_core.h"
#include <cmath>
#include <algorithm>

namespace light_core {

// Convert Color Temperature (Kelvin 2000K .. 6500K) to normalized RGB floats
void kelvinToRgbFloat(uint16_t kelvin, float &r, float &g, float &b) {
  uint16_t k = std::min((uint16_t)6500, std::max((uint16_t)2000, kelvin));
  float temp = k / 100.0f;
  float calcR, calcG, calcB;

  // Calculate Red
  if (temp <= 66.0f) {
    calcR = 255.0f;
  } else {
    calcR = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);
  }

  // Calculate Green
  if (temp <= 66.0f) {
    calcG = 99.4708025861f * std::log(temp) - 161.1195681661f;
  } else {
    calcG = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);
  }

  // Calculate Blue
  if (temp >= 66.0f) {
    calcB = 255.0f;
  } else if (temp <= 19.0f) {
    calcB = 0.0f;
  } else {
    calcB = 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;
  }

  r = std::min(1.0f, std::max(0.0f, calcR / 255.0f));
  g = std::min(1.0f, std::max(0.0f, calcG / 255.0f));
  b = std::min(1.0f, std::max(0.0f, calcB / 255.0f));
}

// Continuous 6-segment floating-point color wheel (hueNorm in 0.0f..1.0f)
void hueToRgbFloat(float hueNorm, float &r, float &g, float &b) {
  hueNorm = std::fmod(hueNorm, 1.0f);
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
uint32_t applyGamma12(float normalized, uint32_t maxDuty) {
  normalized = std::min(1.0f, std::max(0.0f, normalized));
  if (normalized <= 0.0001f) return 0;
  float gammaCorrected = std::pow(normalized, 2.8f);
  return (uint32_t)std::round(gammaCorrected * maxDuty);
}

LightCore::LightCore() : m_mutex(nullptr) {}

LightCore::~LightCore() {
#if !defined(ARDUINO)
  if (m_mutex) {
    vSemaphoreDelete(m_mutex);
  }
#endif
}

void LightCore::init() {
#if !defined(ARDUINO)
  m_mutex = xSemaphoreCreateMutex();
#endif
}

void LightCore::lock() {
#if !defined(ARDUINO)
  if (m_mutex) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
  }
#endif
}

void LightCore::unlock() {
#if !defined(ARDUINO)
  if (m_mutex) {
    xSemaphoreGive(m_mutex);
  }
#endif
}

LightState LightCore::getState() {
  lock();
  LightState copy = m_state;
  unlock();
  return copy;
}

void LightCore::updateState(const LightState &newState, ControlProvider *source) {
  lock();
  m_state = newState;
  LightState copy = m_state;
  unlock();

  for (auto *provider : m_providers) {
    if (provider != source && provider != nullptr) {
      provider->onStateChanged(copy);
    }
  }
}

void LightCore::registerProvider(ControlProvider *provider) {
  if (provider != nullptr) {
    m_providers.push_back(provider);
  }
}

void LightCore::getTargetRgb(float &targetR, float &targetG, float &targetB, float deltaSec) {
  lock();
  LightState st = m_state;
  unlock();

  targetR = st.r / 255.0f;
  targetG = st.g / 255.0f;
  targetB = st.b / 255.0f;

  if (!st.power) {
    targetR = 0.0f;
    targetG = 0.0f;
    targetB = 0.0f;
    return;
  }

  if (st.mode == "white") {
    kelvinToRgbFloat(st.colorTemp, targetR, targetG, targetB);
  } else if (st.effect == "hue_cycle") {
    const float cycleFreqHz = 0.02f + (st.speed - 1) * (0.48f / 99.0f);
    m_animHue += deltaSec * cycleFreqHz;
    if (m_animHue >= 1.0f) m_animHue -= 1.0f;
    hueToRgbFloat(m_animHue, targetR, targetG, targetB);
  } else if (st.effect == "breathe") {
    const float breatheFreqHz = 0.1f + (st.speed - 1) * (1.9f / 99.0f);
    m_breathePhase += deltaSec * breatheFreqHz * 6.2831853f;
    if (m_breathePhase >= 6.2831853f) m_breathePhase -= 6.2831853f;
    const float mult = (std::sin(m_breathePhase) + 1.0f) * 0.5f;
    targetR *= mult;
    targetG *= mult;
    targetB *= mult;
  } else if (st.effect == "strobe") {
    const float strobeInterval = 0.3f - (st.speed - 1) * (0.28f / 99.0f);
    m_strobeTimer += deltaSec;
    if (m_strobeTimer >= strobeInterval) {
      m_strobeTimer = 0.0f;
      m_strobeState = !m_strobeState;
    }
    if (!m_strobeState) {
      targetR = 0.0f;
      targetG = 0.0f;
      targetB = 0.0f;
    }
  }
}

}  // namespace light_core
