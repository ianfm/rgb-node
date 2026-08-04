#include "web_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "config.h"

namespace web_server {

static AsyncWebServer server(config::kHttpPort);
static AsyncWebSocket ws("/ws");
static WiFiMulti wifiMulti;
static StateCallback g_stateCallback = nullptr;
static LightState g_state;
static bool g_usingStaMode = false;

static String serializeStateJson(const LightState &st) {
  JsonDocument doc;
  doc["power"] = st.power;
  doc["r"] = st.r;
  doc["g"] = st.g;
  doc["b"] = st.b;
  doc["brightness"] = st.brightness;
  doc["mode"] = st.mode;
  doc["colorTemp"] = st.colorTemp;
  doc["warmth"] = st.warmth;
  doc["effect"] = st.effect;
  doc["speed"] = st.speed;
  doc["musicSensitivity"] = st.musicSensitivity;
  doc["noiseCutoff"] = st.noiseCutoff;
  doc["headroom"] = st.headroom;
  doc["responseAgility"] = st.responseAgility;
  doc["beatSens"] = st.beatSens;
  doc["beatDecay"] = st.beatDecay;
  doc["pitchLowHz"] = st.pitchLowHz;
  doc["pitchHighHz"] = st.pitchHighHz;
  doc["pitchSmooth"] = st.pitchSmooth;
  doc["ambientGlow"] = st.ambientGlow;
  doc["useLogScale"] = st.useLogScale;
  doc["seq"] = st.seq;
  String out;
  serializeJson(doc, out);
  return out;
}

static void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len) {
    data[len] = 0;
    if (data[0] == '{') {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, (char *)data);
      if (!err) {
        if (!doc["power"].isNull()) g_state.power = doc["power"].as<bool>();
        if (!doc["r"].isNull()) g_state.r = doc["r"].as<uint8_t>();
        if (!doc["g"].isNull()) g_state.g = doc["g"].as<uint8_t>();
        if (!doc["b"].isNull()) g_state.b = doc["b"].as<uint8_t>();
        if (!doc["brightness"].isNull()) g_state.brightness = doc["brightness"].as<uint8_t>();
        if (!doc["mode"].isNull()) g_state.mode = doc["mode"].as<String>();
        if (!doc["colorTemp"].isNull()) g_state.colorTemp = doc["colorTemp"].as<uint16_t>();
        if (!doc["warmth"].isNull()) g_state.warmth = doc["warmth"].as<uint8_t>();
        if (!doc["effect"].isNull()) g_state.effect = doc["effect"].as<String>();
        if (!doc["speed"].isNull()) g_state.speed = doc["speed"].as<uint8_t>();
        if (!doc["musicSensitivity"].isNull()) g_state.musicSensitivity = doc["musicSensitivity"].as<uint8_t>();
        if (!doc["noiseCutoff"].isNull()) g_state.noiseCutoff = doc["noiseCutoff"].as<uint8_t>();
        if (!doc["headroom"].isNull()) g_state.headroom = doc["headroom"].as<uint8_t>();
        if (!doc["responseAgility"].isNull()) g_state.responseAgility = doc["responseAgility"].as<uint8_t>();
        if (!doc["beatSens"].isNull()) g_state.beatSens = doc["beatSens"].as<uint8_t>();
        if (!doc["beatDecay"].isNull()) g_state.beatDecay = doc["beatDecay"].as<uint16_t>();
        if (!doc["pitchLowHz"].isNull()) g_state.pitchLowHz = doc["pitchLowHz"].as<uint16_t>();
        if (!doc["pitchHighHz"].isNull()) g_state.pitchHighHz = doc["pitchHighHz"].as<uint16_t>();
        if (!doc["pitchSmooth"].isNull()) g_state.pitchSmooth = doc["pitchSmooth"].as<uint8_t>();
        if (!doc["ambientGlow"].isNull()) g_state.ambientGlow = doc["ambientGlow"].as<uint8_t>();
        if (!doc["useLogScale"].isNull()) g_state.useLogScale = doc["useLogScale"].as<bool>();
        if (!doc["seq"].isNull()) g_state.seq = doc["seq"].as<uint32_t>();

        if (g_stateCallback) {
          g_stateCallback(g_state);
        }
        broadcastState(g_state);
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
      client->text(serializeStateJson(g_state));
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

void init(StateCallback onStateChange, const LightState &initialState) {
  g_stateCallback = onStateChange;
  g_state = initialState;

  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  }

  WiFi.setHostname(config::kHostname);
  WiFi.mode(WIFI_STA);

  int configuredNetworks = 0;

  if (strlen(config::kWifiSsid) > 0 &&
      strcmp(config::kWifiSsid, "Your_WiFi_SSID") != 0 &&
      strcmp(config::kWifiSsid, "Your_Primary_WiFi_SSID") != 0) {
    wifiMulti.addAP(config::kWifiSsid, config::kWifiPass);
    configuredNetworks++;
    Serial.printf("Added Wi-Fi Network #1: %s\n", config::kWifiSsid);
  }

  if (strlen(config::kWifiSsid2) > 0 &&
      strcmp(config::kWifiSsid2, "Your_Secondary_WiFi_SSID") != 0) {
    wifiMulti.addAP(config::kWifiSsid2, config::kWifiPass2);
    configuredNetworks++;
    Serial.printf("Added Wi-Fi Network #2: %s\n", config::kWifiSsid2);
  }

  if (configuredNetworks > 0) {
    Serial.println("Scanning and connecting to Wi-Fi network...");
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
    Serial.println("Could not connect to configured Wi-Fi. Starting AP mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config::kApSsid, config::kApPass);
    Serial.printf("Access Point started! AP IP: %s\n",
                  WiFi.softAPIP().toString().c_str());
  }

  if (MDNS.begin(config::kHostname)) {
    Serial.printf("mDNS responder started! Access at http://%s.local/\n", config::kHostname);
  }

  // Setup ArduinoOTA for CLI wireless uploads
  ArduinoOTA.setHostname(config::kHostname);
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("ArduinoOTA Start: " + type);
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nArduinoOTA End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("ArduinoOTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("ArduinoOTA Error[%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA listener active");

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // GET /api/status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["version"] = "1.0.1-ota";
    doc["power"] = g_state.power;
    doc["r"] = g_state.r;
    doc["g"] = g_state.g;
    doc["b"] = g_state.b;
    doc["brightness"] = g_state.brightness;
    doc["lightMode"] = g_state.mode;
    doc["colorTemp"] = g_state.colorTemp;
    doc["warmth"] = g_state.warmth;
    doc["effect"] = g_state.effect;
    doc["speed"] = g_state.speed;
    doc["musicSensitivity"] = g_state.musicSensitivity;
    doc["ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString()
                                                : WiFi.softAPIP().toString();
    doc["ssid"] = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : config::kApSsid;
    doc["mode"] = (WiFi.status() == WL_CONNECTED) ? "STA" : "AP";
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // POST /api/state
  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler(
      "/api/state", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject doc = json.as<JsonObject>();
        if (!doc.isNull()) {
          if (!doc["power"].isNull()) g_state.power = doc["power"].as<bool>();
          if (!doc["r"].isNull()) g_state.r = doc["r"].as<uint8_t>();
          if (!doc["g"].isNull()) g_state.g = doc["g"].as<uint8_t>();
          if (!doc["b"].isNull()) g_state.b = doc["b"].as<uint8_t>();
          if (!doc["brightness"].isNull()) g_state.brightness = doc["brightness"].as<uint8_t>();
          if (!doc["lightMode"].isNull()) g_state.mode = doc["lightMode"].as<String>();
          if (!doc["colorTemp"].isNull()) g_state.colorTemp = doc["colorTemp"].as<uint16_t>();
          if (!doc["warmth"].isNull()) g_state.warmth = doc["warmth"].as<uint8_t>();
          if (!doc["effect"].isNull()) g_state.effect = doc["effect"].as<String>();
          if (!doc["speed"].isNull()) g_state.speed = doc["speed"].as<uint8_t>();
          if (!doc["musicSensitivity"].isNull()) g_state.musicSensitivity = doc["musicSensitivity"].as<uint8_t>();

          if (g_stateCallback) {
            g_stateCallback(g_state);
          }
          broadcastState(g_state);
          request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
      });
  server.addHandler(handler);

  // POST /update Web UI Drag-and-Drop OTA Upload
  server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", shouldReboot ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
        if (shouldReboot) {
          delay(500);
          ESP.restart();
        }
      },
      [](AsyncWebServerRequest *request, String filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        if (!index) {
          Serial.printf("HTTP OTA Update Start: %s\n", filename.c_str());
          int command = U_FLASH;
          if (filename.indexOf("littlefs") >= 0 || filename.indexOf("spiffs") >= 0) {
            command = U_SPIFFS;
          }
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {
            Update.printError(Serial);
          }
        }
        if (!Update.hasError()) {
          if (Update.write(data, len) != len) {
            Update.printError(Serial);
          }
        }
        if (final) {
          if (Update.end(true)) {
            Serial.printf("HTTP OTA Update Success: %u B\n", index + len);
          } else {
            Update.printError(Serial);
          }
        }
      });

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  server.begin();
  Serial.println("HTTP Web Server & WebSockets started");
}

void loop() {
  ws.cleanupClients();
  ArduinoOTA.handle();
  if (g_usingStaMode) {
    wifiMulti.run();
  }
}

void broadcastState(const LightState &state) {
  g_state = state;
  ws.textAll(serializeStateJson(g_state));
}

}  // namespace web_server
