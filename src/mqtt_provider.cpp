#include "mqtt_provider.h"
#include <cstdio>
#include <cstring>
#include <ArduinoJson.h>

namespace mqtt_provider {

static void mqttTaskFunc(void *param) {
  MqttProvider *provider = static_cast<MqttProvider *>(param);
  while (true) {
    provider->processLoop();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

MqttProvider::MqttProvider(light_core::LightCore *core, const MqttConfig &cfg)
    : m_core(core), m_config(cfg), m_mqttClient(1024) {
  m_mutex = xSemaphoreCreateMutex();
}

void MqttProvider::init() {
  if (m_core) {
    m_core->registerProvider(this);
  }

  m_wifiClient.setTimeout(150);
  m_mqttClient.setTimeout(150);
  m_mqttClient.begin(m_config.host.c_str(), m_config.port, m_wifiClient);
  m_mqttClient.onMessage([this](String &topic, String &payload) {
    this->handleMessage(topic, payload);
  });

  xTaskCreate(mqttTaskFunc, "mqtt_task", 4096, this, 1, nullptr);
}

void MqttProvider::loop() {
  // Main looper remains 100% non-blocking! MQTT processing runs in background task.
}

void MqttProvider::processLoop() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (m_mutex && xSemaphoreTake(m_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    m_mqttClient.loop();

    if (!m_mqttClient.connected()) {
      uint32_t now = millis();
      if (now - m_lastReconnectAttempt > 15000) {
        m_lastReconnectAttempt = now;
        Serial.println("[MQTT] Connecting to MQTT broker in background...");
        if (m_mqttClient.connect("rgb-node-esp32c3", m_config.username.c_str(), m_config.password.c_str())) {
          Serial.println("[MQTT] Connected successfully to broker!");
          publishDiscoveryPayload();
          m_mqttClient.subscribe("rgb-node/light/switch");
          if (m_core) {
            publishState(m_core->getState());
          }
        }
      }
    }
    xSemaphoreGive(m_mutex);
  }
}

void MqttProvider::setConfig(const MqttConfig &cfg) {
  m_config = cfg;
}

bool MqttProvider::isConnected() {
  return m_mqttClient.connected();
}

void MqttProvider::publishDiscoveryPayload() {
  JsonDocument doc;
  doc["name"] = "RGB Node";
  doc["unique_id"] = "rgb_node_esp32c3";
  doc["cmd_t"] = "rgb-node/light/switch";
  doc["stat_t"] = "rgb-node/light/status";
  doc["schema"] = "json";
  doc["brightness"] = true;
  doc["color_mode"] = true;

  JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
  modes.add("color_temp");
  modes.add("rgb");

  doc["min_mireds"] = 153; // 6500K
  doc["max_mireds"] = 500; // 2000K
  doc["effect"] = true;

  JsonArray effects = doc["effect_list"].to<JsonArray>();
  effects.add("static");
  effects.add("hue_cycle");
  effects.add("breathe");
  effects.add("candle");
  effects.add("strobe");
  effects.add("music_spectrum");
  effects.add("music_pulse");

  JsonObject dev = doc["device"].to<JsonObject>();
  JsonArray ids = dev["identifiers"].to<JsonArray>();
  ids.add("rgb-node-esp32c3");
  dev["name"] = "RGB Node LED Controller";
  dev["model"] = "ESP32-C3 Super Mini";
  dev["manufacturer"] = "Custom Firmware";

  String out;
  serializeJson(doc, out);
  m_mqttClient.publish("homeassistant/light/rgb-node/config", out, true, 0);
  Serial.println("[MQTT] Discovery payload published to homeassistant/light/rgb-node/config");
}

void MqttProvider::publishState(const light_core::LightState &state) {
  if (m_mutex && xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (m_mqttClient.connected()) {
      JsonDocument doc;
      doc["state"] = state.power ? "ON" : "OFF";
      doc["brightness"] = state.brightness;

      if (state.mode == "white") {
        doc["color_mode"] = "color_temp";
        uint32_t mireds = 1000000 / (state.colorTemp > 0 ? state.colorTemp : 2700);
        doc["color_temp"] = mireds;
      } else {
        doc["color_mode"] = "rgb";
        JsonObject rgb = doc["color"].to<JsonObject>();
        rgb["r"] = state.r;
        rgb["g"] = state.g;
        rgb["b"] = state.b;
      }

      doc["effect"] = state.effect.c_str();

      String out;
      serializeJson(doc, out);
      m_mqttClient.publish("rgb-node/light/status", out, false, 0);
    }
    xSemaphoreGive(m_mutex);
  }
}

void MqttProvider::handleMessage(String &topic, String &payload) {
  Serial.printf("[MQTT] Message arrived [%s]: %s\n", topic.c_str(), payload.c_str());

  if (topic == "rgb-node/light/switch" && m_core) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return;

    light_core::LightState st = m_core->getState();

    if (doc.containsKey("state")) {
      const char *s = doc["state"];
      st.power = (strcmp(s, "ON") == 0);
    }
    if (doc.containsKey("brightness")) {
      st.brightness = doc["brightness"];
    }
    if (doc.containsKey("color_mode")) {
      const char *cm = doc["color_mode"];
      if (strcmp(cm, "color_temp") == 0) {
        st.mode = "white";
      } else if (strcmp(cm, "rgb") == 0) {
        st.mode = "color";
      }
    }
    if (doc.containsKey("color_temp")) {
      uint32_t mireds = doc["color_temp"];
      if (mireds > 0) {
        st.colorTemp = 1000000 / mireds;
        st.mode = "white";
      }
    }
    if (doc.containsKey("color")) {
      JsonObject color = doc["color"];
      st.r = color["r"] | st.r;
      st.g = color["g"] | st.g;
      st.b = color["b"] | st.b;
      st.mode = "color";
    }
    if (doc.containsKey("effect")) {
      st.effect = doc["effect"].as<std::string>();
    }

    m_core->updateState(st, this);
    publishState(st);
  }
}

void MqttProvider::onStateChanged(const light_core::LightState &state) {
  publishState(state);
}

}  // namespace mqtt_provider
