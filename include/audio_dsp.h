#pragma once

#include <Arduino.h>
#include <cstdint>

namespace audio_dsp {

struct AudioBands {
  float bass = 0.0f;        // Normalized 0.0f to 1.0f (20 Hz - 250 Hz)
  float mid = 0.0f;         // Normalized 0.0f to 1.0f (250 Hz - 4000 Hz)
  float treble = 0.0f;      // Normalized 0.0f to 1.0f (4000 Hz - 8000 Hz)
  float totalAmp = 0.0f;    // Normalized 0.0f to 1.0f total volume envelope
  float logAmp = 0.0f;      // Logarithmic (dB) volume envelope
  bool beat = false;        // Transient beat detected on bass channel
  float dominantHue = 0.0f; // Continuous hue (0.0f - 1.0f) based on dominant pitch
};

struct DspConfig {
  uint8_t sensitivity = 50;   // 1..100
  uint8_t noiseCutoff = 8;    // 0..25 (%)
  uint8_t headroom = 150;     // 100..250 (%) peak compression factor
  uint8_t beatSens = 45;      // 10..90 (%)
  uint16_t pitchLowHz = 120;  // 80..500 Hz
  uint16_t pitchHighHz = 2400;// 1000..3500 Hz
  uint8_t pitchSmooth = 8;    // 1..50
};

// Initialize I2S hardware peripheral for INMP441 MEMS microphone and start DSP task
void init(uint8_t pinBclk, uint8_t pinWs, uint8_t pinDin);

// Read current smoothed audio bands (thread-safe)
AudioBands getBands();

// Set DSP configuration parameters
void setConfig(const DspConfig &cfg);

}  // namespace audio_dsp
