#include <cmath>
#include <cstring>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "nvs_flash.h"

#include "config.h"
#include "light_core.h"
#include "web_server_native.h"
#include "audio_dsp_native.h"

static const char *TAG = "main_native";

static light_core::LightCore g_core;

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing rgb-node native ESP-IDF control plane...");

    web_server_native::init(&g_core);
    audio_dsp_native::init(config::kPinI2sBclk, config::kPinI2sWs, config::kPinI2sDin);

    ESP_LOGI(TAG, "rgb-node native ESP-IDF core running successfully.");
}
