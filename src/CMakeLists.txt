idf_component_register(
    SRCS "main_native.cpp" "light_core.cpp" "web_server_native.cpp" "audio_dsp_native.cpp"
    INCLUDE_DIRS "../include"
    REQUIRES nvs_flash esp_wifi esp_event esp_http_server littlefs driver mdns json
)
