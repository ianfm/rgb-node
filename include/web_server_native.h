#pragma once

#include "light_core.h"

namespace web_server_native {

// Initialize native ESP-IDF NVS, LittleFS, Wi-Fi, mDNS, esp_http_server REST API, and WebSockets
void init(light_core::LightCore *core);

// Broadcast updated LightState to all active WebSocket clients
void broadcastState(const light_core::LightState &state);

}  // namespace web_server_native
