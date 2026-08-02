#include "web_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "config.h"

namespace web_server {

static AsyncWebServer server(config::kHttpPort);
static AsyncWebSocket ws("/ws");
static WiFiMulti wifiMulti;
static ColorCallback g_colorCallback = nullptr;
static uint8_t g_currentR = 0;
static uint8_t g_currentG = 0;
static uint8_t g_currentB = 0;
static bool g_usingStaMode = false;

static void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len) {
    data[len] = 0;
    // Check if message is JSON e.g. {"r":255,"g":0,"b":128} or binary "0,255,0,128"
    if (data[0] == '{') {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, (char *)data);
      if (!err) {
        uint8_t r = doc["r"] | 0;
        uint8_t g = doc["g"] | 0;
        uint8_t b = doc["b"] | 0;
        g_currentR = r;
        g_currentG = g;
        g_currentB = b;
        if (g_colorCallback) {
          g_colorCallback(r, g, b);
        }
      }
    } else {
      // Legacy format "<stripIndex>,<R>,<G>,<B>"
      int stripIdx = 0, r = 0, g = 0, b = 0;
      if (sscanf((char *)data, "%d,%d,%d,%d", &stripIdx, &r, &g, &b) >= 4) {
        g_currentR = (uint8_t)r;
        g_currentG = (uint8_t)g;
        g_currentB = (uint8_t)b;
        if (g_colorCallback) {
          g_colorCallback(g_currentR, g_currentG, g_currentB);
        }
      }
    }
  }
}

static void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                    AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(),
                    client->remoteIP().toString().c_str());
      // Send current state to newly connected client
      broadcastState(g_currentR, g_currentG, g_currentB);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void init(ColorCallback onColorChange) {
  g_colorCallback = onColorChange;

  // Mount LittleFS filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  }

  WiFi.setHostname(config::kHostname);
  WiFi.mode(WIFI_STA);

  int configuredNetworks = 0;

  // Register primary Wi-Fi network if configured
  if (strlen(config::kWifiSsid) > 0 &&
      strcmp(config::kWifiSsid, "Your_WiFi_SSID") != 0 &&
      strcmp(config::kWifiSsid, "Your_Primary_WiFi_SSID") != 0) {
    wifiMulti.addAP(config::kWifiSsid, config::kWifiPass);
    configuredNetworks++;
    Serial.printf("Added Wi-Fi Network #1: %s\n", config::kWifiSsid);
  }

  // Register secondary Wi-Fi network if configured
  if (strlen(config::kWifiSsid2) > 0 &&
      strcmp(config::kWifiSsid2, "Your_Secondary_WiFi_SSID") != 0) {
    wifiMulti.addAP(config::kWifiSsid2, config::kWifiPass2);
    configuredNetworks++;
    Serial.printf("Added Wi-Fi Network #2: %s\n", config::kWifiSsid2);
  }

  if (configuredNetworks > 0) {
    Serial.println("Scanning and connecting to strongest Wi-Fi network...");
    uint8_t attempts = 0;
    while (wifiMulti.run() != WL_CONNECTED && attempts < 25) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    g_usingStaMode = true;
    Serial.printf("Wi-Fi connected to SSID: %s | IP: %s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    g_usingStaMode = false;
    Serial.println("Could not connect to any configured Wi-Fi networks.");
    Serial.println("Starting Access Point (SoftAP)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config::kApSsid, config::kApPass);
    Serial.printf("Access Point started! AP IP address: %s\n",
                  WiFi.softAPIP().toString().c_str());
  }

  // Start mDNS responder (http://rgb-node.local)
  if (MDNS.begin(config::kHostname)) {
    Serial.printf("mDNS responder started! Access at http://%s.local/\n", config::kHostname);
  }

  // Setup WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // REST API: GET /api/status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["r"] = g_currentR;
    doc["g"] = g_currentG;
    doc["b"] = g_currentB;
    doc["ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString()
                                                : WiFi.softAPIP().toString();
    doc["ssid"] = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : config::kApSsid;
    doc["mode"] = (WiFi.status() == WL_CONNECTED) ? "STA" : "AP";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // REST API: POST /api/rgb
  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler(
      "/api/rgb", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        if (!jsonObj["r"].isNull() && !jsonObj["g"].isNull() &&
            !jsonObj["b"].isNull()) {
          g_currentR = jsonObj["r"].as<uint8_t>();
          g_currentG = jsonObj["g"].as<uint8_t>();
          g_currentB = jsonObj["b"].as<uint8_t>();
          if (g_colorCallback) {
            g_colorCallback(g_currentR, g_currentG, g_currentB);
          }
          broadcastState(g_currentR, g_currentG, g_currentB);
          request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
          request->send(400, "application/json",
                        "{\"error\":\"Missing r, g, or b\"}");
        }
      });
  server.addHandler(handler);

  // Serve LittleFS static files
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Fallback for SPA routing / unhandled requests
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  // Add CORS headers for local web dev preview
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                       "Content-Type");

  server.begin();
  Serial.println("HTTP Web Server & WebSockets started");
}

void loop() {
  ws.cleanupClients();
  // Maintain Wi-Fi multi-network connection if in STA mode
  if (g_usingStaMode) {
    wifiMulti.run();
  }
}

void broadcastState(uint8_t r, uint8_t g, uint8_t b) {
  g_currentR = r;
  g_currentG = g;
  g_currentB = b;
  JsonDocument doc;
  doc["r"] = r;
  doc["g"] = g;
  doc["b"] = b;
  String message;
  serializeJson(doc, message);
  ws.textAll(message);
}

}  // namespace web_server
