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
  static float maxPeak = 0.05f;
  static float bassAvg = 0.0f;
  static float smoothedHue = 0.0f;
  
  while (true) {
    size_t bytesRead = 0;
    if (rx_handle != nullptr) {
      esp_err_t err = i2s_channel_read(rx_handle, rawSamples, sizeof(rawSamples), &bytesRead, pdMS_TO_TICKS(50));
      if (err == ESP_OK && bytesRead > 0) {
        size_t samplesRead = bytesRead / sizeof(int32_t);
        
        // Remove DC offset and compute RMS volume + zero crossing frequency estimate
        float sumSamples = 0.0f;
        for (size_t i = 0; i < samplesRead; i++) {
          float s = (float)(rawSamples[i] >> 8) / 8388608.0f;
          sumSamples += s;
        }
        float dcOffset = sumSamples / samplesRead;

        float sumEnergy = 0.0f;
        float zeroCrossings = 0.0f;
        float prevSample = 0.0f;

        // Band filters
        float sumBass = 0.0f, sumMid = 0.0f, sumTreble = 0.0f;
        static float lpBass = 0.0f, hpTreble = 0.0f;

        for (size_t i = 0; i < samplesRead; i++) {
          float s = ((float)(rawSamples[i] >> 8) / 8388608.0f) - dcOffset;
          sumEnergy += s * s;

          // Zero-crossing count for pitch frequency estimation
          if (i > 0 && ((s >= 0.0f && prevSample < 0.0f) || (s < 0.0f && prevSample >= 0.0f))) {
            zeroCrossings += 1.0f;
          }
          prevSample = s;

          // 3-Band splitting
          lpBass += (s - lpBass) * 0.12f;                  // ~300Hz LP
          hpTreble += (s - hpTreble) * 0.45f;              // ~2.5kHz HP
          float trebleS = s - hpTreble;
          float midS = s - lpBass - trebleS;

          sumBass += lpBass * lpBass;
          sumMid += midS * midS;
          sumTreble += trebleS * trebleS;
        }

        float rmsTotal = sqrtf(sumEnergy / samplesRead) * g_sensitivityMult;
        float rmsBass = sqrtf(sumBass / samplesRead) * g_sensitivityMult;
        float rmsMid = sqrtf(sumMid / samplesRead) * g_sensitivityMult;
        float rmsTreble = sqrtf(sumTreble / samplesRead) * g_sensitivityMult;

        // Dynamic AGC with floor
        if (rmsTotal > maxPeak) {
          maxPeak = rmsTotal;
        } else {
          maxPeak = fmaxf(0.02f, maxPeak * 0.992f);
        }

        float normTotal = constrain(rmsTotal / maxPeak, 0.0f, 1.0f);
        float normBass = constrain(rmsBass / maxPeak, 0.0f, 1.0f);
        float normMid = constrain(rmsMid / maxPeak, 0.0f, 1.0f);
        float normTreble = constrain(rmsTreble / maxPeak, 0.0f, 1.0f);

        // Beat detection
        bassAvg += (normBass - bassAvg) * 0.1f;
        bool isBeat = (normBass > (bassAvg * 1.45f)) && (normBass > 0.35f);

        // Calculate estimated pitch frequency (Hz) from Zero Crossings:
        // Freq (Hz) = (ZeroCrossings / 2) * (SampleRate / SampleCount)
        float estFreqHz = (zeroCrossings * 0.5f) * (config::kAudioSampleRate / (float)samplesRead);

        // Map pitch frequency to smooth continuous Hue (0.0f Red to 0.75f Purple)
        // 100Hz (Bass) -> 0.0 (Red), 800Hz (Mids) -> 0.33 (Green), 2200Hz+ (Whistle/High Pitch) -> 0.66 (Blue)
        float targetHue = smoothedHue;
        if (normTotal > 0.05f) { // Noise gate
          float freqNorm = constrain((estFreqHz - 120.0f) / 2400.0f, 0.0f, 1.0f);
          targetHue = freqNorm * 0.75f; // Red -> Yellow -> Green -> Cyan -> Blue -> Purple
        }

        // Exponential low-pass temporal smoothing filter to eliminate flicker
        smoothedHue += (targetHue - smoothedHue) * 0.08f;
        smoothedHue = fmodf(smoothedHue, 1.0f);
        if (smoothedHue < 0.0f) smoothedHue += 1.0f;

        if (g_bandsMutex && xSemaphoreTake(g_bandsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          g_currentBands.bass = normBass;
          g_currentBands.mid = normMid;
          g_currentBands.treble = normTreble;
          g_currentBands.totalAmp = normTotal;
          g_currentBands.beat = isBeat;
          g_currentBands.dominantHue = smoothedHue;
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
