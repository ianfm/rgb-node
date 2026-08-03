#include "audio_dsp_native.h"

#include <cmath>
#include <algorithm>
#include <driver/i2s_std.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "config.h"

static const char *TAG = "audio_dsp_native";

namespace audio_dsp_native {

static i2s_chan_handle_t rx_handle = nullptr;
static AudioBands g_currentBands;
static float g_sensitivityMult = 1.0f;
static SemaphoreHandle_t g_bandsMutex = nullptr;

static constexpr size_t kSampleCount = 256;
static int32_t rawSamples[kSampleCount];

void setSensitivity(uint8_t sensitivity) {
  float norm = std::min(100.0f, std::max(1.0f, (float)sensitivity));
  g_sensitivityMult = 0.2f + (norm - 1.0f) * (4.8f / 99.0f);
}

AudioBands getBands() {
  AudioBands bands;
  if (g_bandsMutex && xSemaphoreTake(g_bandsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    bands = g_currentBands;
    xSemaphoreGive(g_bandsMutex);
  }
  return bands;
}

static void audioTask(void *param) {
  static float maxPeak = 100000.0f;
  static float bassAvg = 0.0f;

  while (true) {
    size_t bytesRead = 0;
    if (rx_handle != nullptr) {
      esp_err_t err = i2s_channel_read(rx_handle, rawSamples, sizeof(rawSamples), &bytesRead, pdMS_TO_TICKS(50));
      if (err == ESP_OK && bytesRead > 0) {
        size_t samplesRead = bytesRead / sizeof(int32_t);

        float sumBass = 0.0f, sumMid = 0.0f, sumTreble = 0.0f, sumTotal = 0.0f;
        static float lowpassBass = 0.0f, highpassTreble = 0.0f;

        for (size_t i = 0; i < samplesRead; i++) {
          float sample = (float)(rawSamples[i] >> 8) / 8388608.0f;
          lowpassBass += (sample - lowpassBass) * 0.08f;
          highpassTreble += (sample - highpassTreble) * 0.60f;
          float trebleSample = sample - highpassTreble;
          float midSample = sample - lowpassBass - trebleSample;

          sumBass += lowpassBass * lowpassBass;
          sumMid += midSample * midSample;
          sumTreble += trebleSample * trebleSample;
          sumTotal += sample * sample;
        }

        float rmsBass = std::sqrt(sumBass / samplesRead) * g_sensitivityMult;
        float rmsMid = std::sqrt(sumMid / samplesRead) * g_sensitivityMult;
        float rmsTreble = std::sqrt(sumTreble / samplesRead) * g_sensitivityMult;
        float rmsTotal = std::sqrt(sumTotal / samplesRead) * g_sensitivityMult;

        float currentPeak = std::max({rmsTotal, rmsBass, rmsMid, rmsTreble});
        if (currentPeak > maxPeak) {
          maxPeak = currentPeak;
        } else {
          maxPeak = std::max(0.01f, maxPeak * 0.995f);
        }

        float normBass = std::min(1.0f, std::max(0.0f, rmsBass / maxPeak));
        float normMid = std::min(1.0f, std::max(0.0f, rmsMid / maxPeak));
        float normTreble = std::min(1.0f, std::max(0.0f, rmsTreble / maxPeak));
        float normTotal = std::min(1.0f, std::max(0.0f, rmsTotal / maxPeak));

        bassAvg += (normBass - bassAvg) * 0.1f;
        bool isBeat = (normBass > (bassAvg * 1.45f)) && (normBass > 0.35f);

        float domHue = 0.0f;
        if (normBass > normMid && normBass > normTreble) domHue = 0.0f + 0.15f * (1.0f - normBass);
        else if (normMid >= normBass && normMid >= normTreble) domHue = 0.33f + 0.2f * normMid;
        else domHue = 0.66f + 0.25f * normTreble;

        if (g_bandsMutex && xSemaphoreTake(g_bandsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          g_currentBands.bass = normBass;
          g_currentBands.mid = normMid;
          g_currentBands.treble = normTreble;
          g_currentBands.totalAmp = normTotal;
          g_currentBands.beat = isBeat;
          g_currentBands.dominantHue = domHue;
          xSemaphoreGive(g_bandsMutex);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void init(uint8_t pinBclk, uint8_t pinWs, uint8_t pinDin) {
  g_bandsMutex = xSemaphoreCreateMutex();

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &rx_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2S RX channel: 0x%x", err);
    return;
  }

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config::kAudioSampleRate),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = (gpio_num_t)pinBclk,
          .ws = (gpio_num_t)pinWs,
          .dout = I2S_GPIO_UNUSED,
          .din = (gpio_num_t)pinDin,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  err = i2s_channel_init_std_mode(rx_handle, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init I2S std mode: 0x%x", err);
    return;
  }

  err = i2s_channel_enable(rx_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable I2S channel: 0x%x", err);
    return;
  }

  ESP_LOGI(TAG, "Native ESP-IDF I2S INMP441 audio reader initialized!");
  xTaskCreate(audioTask, "audio_dsp_task", 4096, nullptr, 3, nullptr);
}

}  // namespace audio_dsp_native
