#include "audio_dsp.h"

#include <cmath>
#include <driver/i2s_std.h>
#include "config.h"

namespace audio_dsp {

static i2s_chan_handle_t rx_handle = nullptr;
static AudioBands g_currentBands;
static float g_sensitivityMult = 1.0f;
static SemaphoreHandle_t g_bandsMutex = nullptr;

// 256 sample buffer at 16 kHz = 16ms window
static constexpr size_t kSampleCount = 256;
static int32_t rawSamples[kSampleCount];

void setSensitivity(uint8_t sensitivity) {
  // Sensitivity 1..100 maps to gain multiplier 0.2f .. 5.0f
  float norm = constrain((float)sensitivity, 1.0f, 100.0f);
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
        
        // Remove DC offset and compute band energies using Goertzel / IIR quadrature filters
        float sumBass = 0.0f;
        float sumMid = 0.0f;
        float sumTreble = 0.0f;
        float sumTotal = 0.0f;

        // Simple IIR filter accumulators
        static float lowpassBass = 0.0f;
        static float bandpassMid = 0.0f;
        static float highpassTreble = 0.0f;

        for (size_t i = 0; i < samplesRead; i++) {
          // INMP441 is 24-bit data left-aligned in 32-bit container
          float sample = (float)(rawSamples[i] >> 8) / 8388608.0f; // Normalized -1.0 to 1.0

          // Simple band splitting IIR filters
          lowpassBass += (sample - lowpassBass) * 0.08f;               // ~200Hz LP cutoff @ 16kHz
          highpassTreble += (sample - highpassTreble) * 0.60f;         // High pass tracking
          float trebleSample = sample - highpassTreble;                // >4kHz HP cutoff
          float midSample = sample - lowpassBass - trebleSample;       // 200Hz - 4kHz BP

          sumBass += lowpassBass * lowpassBass;
          sumMid += midSample * midSample;
          sumTreble += trebleSample * trebleSample;
          sumTotal += sample * sample;
        }

        float rmsBass = sqrtf(sumBass / samplesRead) * g_sensitivityMult;
        float rmsMid = sqrtf(sumMid / samplesRead) * g_sensitivityMult;
        float rmsTreble = sqrtf(sumTreble / samplesRead) * g_sensitivityMult;
        float rmsTotal = sqrtf(sumTotal / samplesRead) * g_sensitivityMult;

        // Dynamic AGC (Automatic Gain Control) Peak Tracking with Exponential Decay
        float currentPeak = fmaxf(rmsTotal, fmaxf(rmsBass, fmaxf(rmsMid, rmsTreble)));
        if (currentPeak > maxPeak) {
          maxPeak = currentPeak;
        } else {
          maxPeak = fmaxf(0.01f, maxPeak * 0.995f); // Slow decay
        }

        // Normalize bands to 0.0f .. 1.0f range based on peak envelope
        float normBass = constrain(rmsBass / maxPeak, 0.0f, 1.0f);
        float normMid = constrain(rmsMid / maxPeak, 0.0f, 1.0f);
        float normTreble = constrain(rmsTreble / maxPeak, 0.0f, 1.0f);
        float normTotal = constrain(rmsTotal / maxPeak, 0.0f, 1.0f);

        // Beat detection logic (transient peak in bass band)
        bassAvg += (normBass - bassAvg) * 0.1f;
        bool isBeat = (normBass > (bassAvg * 1.45f)) && (normBass > 0.35f);

        // Dominant Pitch-to-Hue mapping
        float domHue = 0.0f;
        if (normBass > normMid && normBass > normTreble) {
          domHue = 0.0f + 0.15f * (1.0f - normBass); // Red/Orange range
        } else if (normMid >= normBass && normMid >= normTreble) {
          domHue = 0.33f + 0.2f * normMid;           // Green/Cyan range
        } else {
          domHue = 0.66f + 0.25f * normTreble;        // Blue/Purple range
        }

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
    Serial.printf("Failed to create I2S RX channel: 0x%x\n", err);
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
  // Select left channel for INMP441 (L/R pin grounded)
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  err = i2s_channel_init_std_mode(rx_handle, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("Failed to init I2S std mode: 0x%x\n", err);
    return;
  }

  err = i2s_channel_enable(rx_handle);
  if (err != ESP_OK) {
    Serial.printf("Failed to enable I2S channel: 0x%x\n", err);
    return;
  }

  Serial.println("I2S INMP441 Microphone initialized successfully!");

  xTaskCreate(audioTask, "audio_dsp_task", 4096, nullptr, 3, nullptr);
}

}  // namespace audio_dsp
