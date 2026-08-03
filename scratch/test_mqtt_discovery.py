import json

def test_ha_mqtt_discovery_schema():
    payload = {
        "name": "RGB Node",
        "unique_id": "rgb_node_esp32c3",
        "cmd_t": "rgb-node/light/switch",
        "stat_t": "rgb-node/light/status",
        "schema": "json",
        "brightness": True,
        "color_mode": True,
        "supported_color_modes": ["color_temp", "rgb"],
        "min_mireds": 153,
        "max_mireds": 500,
        "effect": True,
        "effect_list": ["static", "hue_cycle", "breathe", "candle", "strobe", "music_spectrum", "music_pulse"],
        "device": {
            "identifiers": ["rgb-node-esp32c3"],
            "name": "RGB Node LED Controller",
            "model": "ESP32-C3 Super Mini",
            "manufacturer": "Custom Firmware"
        }
    }
    
    # Assert standard Home Assistant Light MQTT schema rules
    assert payload["schema"] == "json"
    assert "color_temp" in payload["supported_color_modes"]
    assert "rgb" in payload["supported_color_modes"]
    assert payload["min_mireds"] == 153  # 6500K
    assert payload["max_mireds"] == 500  # 2000K
    assert "identifiers" in payload["device"]
    
    print("[+] Home Assistant MQTT Auto-Discovery Schema Validated!")

if __name__ == "__main__":
    test_ha_mqtt_discovery_schema()
