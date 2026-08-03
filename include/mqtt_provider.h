#pragma once

#include "light_core.h"
#include <string>

namespace mqtt_provider {

struct MqttConfig {
  bool enabled = true;
  std::string host = "";       // e.g. "192.168.1.100" or "homeassistant.local"
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
  bool isConnected() const;

 private:
  light_core::LightCore *m_core;
  MqttConfig m_config;
  bool m_connected = false;

  void publishDiscoveryPayload();
  void publishState(const light_core::LightState &state);
};

}  // namespace mqtt_provider
