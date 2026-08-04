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
  String mode = "color";     // "color" or "white"
  uint16_t colorTemp = 2700; // 2000K .. 6500K
  uint8_t warmth = 84;       // 0% (6500K cool) .. 100% (2000K warm)
  String effect = "static";  // "static", "hue_cycle", "breathe", "candle", "strobe", "music_spectrum", "music_pulse", "music_amplitude", "music_freq_hue", "music_chill"
  uint8_t speed = 50;        // 1..100
  uint8_t musicSensitivity = 50; // 1..100
  uint8_t noiseCutoff = 8;       // 0..25 (%)
  uint8_t beatSens = 45;         // 10..90 (%)
};

using StateCallback = std::function<void(const LightState &state)>;

// Initialize Wi-Fi, LittleFS static web server, REST API, and WebSockets.
// Pass a callback function that receives updated LightState.
void init(StateCallback onStateChange, const LightState &initialState);

// Periodically clean up WebSocket clients and handle background events.
void loop();

// Broadcast current LightState to all connected web clients.
void broadcastState(const LightState &state);

}  // namespace web_server
