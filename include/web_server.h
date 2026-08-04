#pragma once

#include <Arduino.h>
#include <cstdint>
#include <functional>

namespace web_server {

struct LightState {
  bool power = true;
  uint8_t r = 0;
  uint8_t g = 240;
  uint8_t b = 255;
  uint8_t brightness = 255;
  String mode = "color";          // "color" or "white"
  uint16_t colorTemp = 2700;      // 2000K .. 6500K
  uint8_t warmth = 84;            // 0% (6500K cool) .. 100% (2000K warm)
  String effect = "static";       // "static", "hue_cycle", "breathe", "candle", "strobe", "music_spectrum", "music_pulse", "music_amplitude", "music_freq_hue", "music_chill"
  uint8_t speed = 50;             // 1..100
  uint8_t musicSensitivity = 50;  // 1..100 (Gain)
  uint8_t noiseCutoff = 8;        // 0..25 (%) Noise Floor Cutoff
  uint8_t headroom = 150;         // 100..250 (%) Dynamic Headroom Margin
  uint8_t responseAgility = 50;   // 1..100 (%) PWM Attack/Decay Slew Rate
  uint8_t beatSens = 45;          // 10..90 (%) Beat Detection Sensitivity
  uint16_t beatDecay = 180;       // 20..500 (ms) Beat Pulse Fade Tail
  uint16_t pitchLowHz = 120;      // 80..500 Hz Low Pitch Bound
  uint16_t pitchHighHz = 2400;    // 1000..3500 Hz High Pitch Whistle Bound
  uint8_t pitchSmooth = 8;        // 1..50 Pitch Glide Smoothness
  uint8_t ambientGlow = 0;        // 0..30 (%) Minimum Background Glow
  bool useLogScale = true;        // Logarithmic dB vs Linear Volume Scaling
  uint32_t seq = 0;               // Sequence ID for echo suppression
};

using StateCallback = std::function<void(const LightState &state)>;

void init(StateCallback onStateChange, const LightState &initialState);
void loop();
void broadcastState(const LightState &state);

}  // namespace web_server
