#pragma once

#include <cstdint>
#include <functional>

namespace web_server {

using ColorCallback = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;

// Initialize Wi-Fi, LittleFS static web server, REST API, and WebSockets.
// Pass a callback function that receives updated RGB values.
void init(ColorCallback onColorChange);

// Periodically clean up WebSocket clients and handle background events.
void loop();

// Broadcast current RGB color state to all connected web clients.
void broadcastState(uint8_t r, uint8_t g, uint8_t b);

}  // namespace web_server
