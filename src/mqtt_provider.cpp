#include "mqtt_provider.h"
#include <cstdio>
#include <cstring>
#include "cJSON.h"

namespace mqtt_provider {

MqttProvider::MqttProvider(light_core::LightCore *core, const MqttConfig &cfg)
    : m_core(core), m_config(cfg), m_connected(false) {}

void MqttProvider::init() {
  if (m_core) {
    m_core->registerProvider(this);
  }
}

void MqttProvider::loop() {
  // MQTT reconnect & telemetry polling loop
}

void MqttProvider::setConfig(const MqttConfig &cfg) {
  m_config = cfg;
}

bool MqttProvider::isConnected() const {
  return m_connected;
}

void MqttProvider::publishDiscoveryPayload() {
  // Discovery topic: homeassistant/light/rgb-node/config
  cJSON *doc = cJSON_CreateObject();
  cJSON_AddStringToObject(doc, "name", "RGB Node");
  cJSON_AddStringToObject(doc, "unique_id", "rgb_node_esp32c3");
  cJSON_AddStringToObject(doc, "cmd_t", "rgb-node/light/switch");
  cJSON_AddStringToObject(doc, "stat_t", "rgb-node/light/status");
  cJSON_AddStringToObject(doc, "schema", "json");
  cJSON_AddBoolToObject(doc, "brightness", true);
  cJSON_AddBoolToObject(doc, "color_mode", true);
  
  cJSON *modes = cJSON_CreateArray();
  cJSON_AddItemToArray(modes, cJSON_CreateString("color_temp"));
  cJSON_AddItemToArray(modes, cJSON_CreateString("rgb"));
  cJSON_AddItemToObject(doc, "supported_color_modes", modes);

  cJSON_AddNumberToObject(doc, "min_mireds", 153); // 6500K
  cJSON_AddNumberToObject(doc, "max_mireds", 500); // 2000K
  cJSON_AddBoolToObject(doc, "effect", true);

  cJSON *effects = cJSON_CreateArray();
  cJSON_AddItemToArray(effects, cJSON_CreateString("static"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("hue_cycle"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("breathe"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("candle"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("strobe"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("music_spectrum"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("music_pulse"));
  cJSON_AddItemToObject(doc, "effect_list", effects);

  cJSON *dev = cJSON_CreateObject();
  cJSON *ids = cJSON_CreateArray();
  cJSON_AddItemToArray(ids, cJSON_CreateString("rgb-node-esp32c3"));
  cJSON_AddItemToObject(dev, "identifiers", ids);
  cJSON_AddStringToObject(dev, "name", "RGB Node LED Controller");
  cJSON_AddStringToObject(dev, "model", "ESP32-C3 Super Mini");
  cJSON_AddStringToObject(dev, "manufacturer", "Custom Firmware");
  cJSON_AddItemToObject(doc, "device", dev);

  char *payload = cJSON_PrintUnformatted(doc);
  cJSON_Delete(doc);
  // Publish payload to MQTT broker
  free(payload);
}

void MqttProvider::publishState(const light_core::LightState &state) {
  cJSON *doc = cJSON_CreateObject();
  cJSON_AddStringToObject(doc, "state", state.power ? "ON" : "OFF");
  cJSON_AddNumberToObject(doc, "brightness", state.brightness);

  if (state.mode == "white") {
    cJSON_AddStringToObject(doc, "color_mode", "color_temp");
    uint32_t mireds = 1000000 / state.colorTemp;
    cJSON_AddNumberToObject(doc, "color_temp", mireds);
  } else {
    cJSON_AddStringToObject(doc, "color_mode", "rgb");
    cJSON *rgb = cJSON_CreateObject();
    cJSON_AddNumberToObject(rgb, "r", state.r);
    cJSON_AddNumberToObject(rgb, "g", state.g);
    cJSON_AddNumberToObject(rgb, "b", state.b);
    cJSON_AddItemToObject(doc, "color", rgb);
  }

  cJSON_AddStringToObject(doc, "effect", state.effect.c_str());

  char *payload = cJSON_PrintUnformatted(doc);
  cJSON_Delete(doc);
  // Publish state to rgb-node/light/status
  free(payload);
}

void MqttProvider::onStateChanged(const light_core::LightState &state) {
  publishState(state);
}

}  // namespace mqtt_provider
