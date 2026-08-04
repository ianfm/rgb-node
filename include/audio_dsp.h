#pragma once

#include <Arduino.h>
#include <cstdint>

namespace audio_dsp {

struct AudioBands {
  float bass = 0.0f;        // Normalized 0.0f to 1.0f (20 Hz - 250 Hz)
  float mid = 0.0f;         // Normalized 0.0f to 1.0f (250 Hz - 4000 Hz)
  float treble = 0.0f;      // Normalized 0.0f to 1.0f (4000 Hz - 8000 Hz)
  float totalAmp = 0.0f;    // Normalized 0.0f to 1.0f total volume envelope
  bool beat = false;        // Transient beat detected on bass channel
  float dominantHue = 0.0f; // Continuous hue (0.0f - 1.0f) based on dominant pitch
};

// Initialize I2S hardware peripheral for INMP441 MEMS microphone and start DSP task
void init(uint8_t pinBclk, uint8_t pinWs, uint8_t pinDin);

// Read current smoothed audio bands (thread-safe)
AudioBands getBands();

// Set audio sensitivity scale (1 .. 100)
void setSensitivity(uint8_t sensitivity);

}  // namespace audio_dsp
