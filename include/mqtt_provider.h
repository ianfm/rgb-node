#pragma once

#include "light_core.h"
#include <string>
#include <WiFi.h>
#include <MQTT.h>

namespace mqtt_provider {

struct MqttConfig {
  bool enabled = true;
  std::string host = "192.168.86.188";
  uint16_t port = 1883;
  std::string username = "";
  std::string password = "";
  std::string topicPrefix = "rgb-node";
};

class MqttProvider : public light_core::ControlProvider {
 public:
  MqttProvider(light_core::LightCore *core, const MqttConfig &cfg);
  ~MqttProvider() override = default;

  void init() override;
  void loop() override;
  void onStateChanged(const light_core::LightState &state) override;

  void setConfig(const MqttConfig &cfg);
  bool isConnected();
  void processLoop();

 private:
  light_core::LightCore *m_core;
  MqttConfig m_config;
  WiFiClient m_wifiClient;
  MQTTClient m_mqttClient;
  uint32_t m_lastReconnectAttempt = 0;
  bool m_discoveryPublished = false;
  SemaphoreHandle_t m_mutex = nullptr;

  void publishDiscoveryPayload();
  void publishState(const light_core::LightState &state);
  void handleMessage(String &topic, String &payload);
};

}  // namespace mqtt_provider
