#pragma once

#include <cstdint>

namespace audio_dsp_native {

struct AudioBands {
  float bass = 0.0f;
  float mid = 0.0f;
  float treble = 0.0f;
  float totalAmp = 0.0f;
  bool beat = false;
  float dominantHue = 0.0f;
};

// Initialize native ESP-IDF I2S DMA channel for INMP441 microphone
void init(uint8_t pinBclk, uint8_t pinWs, uint8_t pinDin);

// Read current audio bands (thread-safe)
AudioBands getBands();

// Set audio sensitivity (1..100)
void setSensitivity(uint8_t sensitivity);

}  // namespace audio_dsp_native
