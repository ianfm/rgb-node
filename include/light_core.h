#pragma once

#include <cstdint>
#include <vector>
#include <string>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

namespace light_core {

struct LightState {
  bool power = true;
  uint8_t r = 0;
  uint8_t g = 240;
  uint8_t b = 255;
  uint8_t brightness = 255;
  std::string mode = "white";      // "color" or "white"
  uint16_t colorTemp = 2700;       // 2000K .. 6500K
  uint8_t warmth = 84;             // 0% .. 100%
  std::string effect = "static";   // "static", "hue_cycle", "breathe", "candle", "strobe", etc.
  uint8_t speed = 50;              // 1 .. 100
  uint8_t musicSensitivity = 50;   // 1 .. 100
};

// Abstract Control Provider Base Class for Observers
class ControlProvider {
 public:
  virtual ~ControlProvider() = default;
  virtual void init() = 0;
  virtual void loop() = 0;
  virtual void onStateChanged(const LightState &state) = 0;
};

// Core Color Engine Math Utilities (Pure, Testable C++ Functions)
void kelvinToRgbFloat(uint16_t kelvin, float &r, float &g, float &b);
void hueToRgbFloat(float hueNorm, float &r, float &g, float &b);
uint32_t applyGamma12(float normalized, uint32_t maxDuty = 4095);

// Unified Thread-Safe Observer State Bus Manager
class LightCore {
 public:
  LightCore();
  ~LightCore();

  void init();
  LightState getState();
  void updateState(const LightState &newState, ControlProvider *source = nullptr);
  void registerProvider(ControlProvider *provider);

  // Compute current instantaneous RGB target floats (0.0f .. 1.0f)
  void getTargetRgb(float &targetR, float &targetG, float &targetB, float deltaSec);

 private:
  LightState m_state;
  std::vector<ControlProvider*> m_providers;
#if defined(ARDUINO)
  void *m_mutex;
#else
  SemaphoreHandle_t m_mutex;
#endif
  float m_animHue = 0.0f;
  float m_breathePhase = 0.0f;
  bool m_strobeState = false;
  float m_strobeTimer = 0.0f;

  void lock();
  void unlock();
};

}  // namespace light_core
