#include "audio_dsp.h"

#include <cmath>
#include <driver/i2s_std.h>
#include "config.h"

namespace audio_dsp {

static i2s_chan_handle_t rx_handle = nullptr;
static AudioBands g_currentBands;
static DspConfig g_dspConfig;
static SemaphoreHandle_t g_bandsMutex = nullptr;

// 256 sample buffer at 16 kHz = 16ms window
static constexpr size_t kSampleCount = 256;
static int32_t rawSamples[kSampleCount];

void setConfig(const DspConfig &cfg) {
  if (g_bandsMutex && xSemaphoreTake(g_bandsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    g_dspConfig = cfg;
    xSemaphoreGive(g_bandsMutex);
  }
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

        // Fetch current DSP Config thread-safely
        DspConfig cfg;
        if (g_bandsMutex && xSemaphoreTake(g_bandsMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
          cfg = g_dspConfig;
          xSemaphoreGive(g_bandsMutex);
        } else {
          cfg = g_dspConfig;
        }

        float sensMult = 0.2f + (constrain((float)cfg.sensitivity, 1.0f, 100.0f) - 1.0f) * (4.8f / 99.0f);
        float noiseGateCutoff = (float)cfg.noiseCutoff / 100.0f;
        float headroomMult = (float)cfg.headroom / 100.0f; // e.g. 1.5x peak margin to prevent moderate whistling clipping

        // Remove DC offset
        float sumSamples = 0.0f;
        for (size_t i = 0; i < samplesRead; i++) {
          float s = (float)(rawSamples[i] >> 8) / 8388608.0f;
          sumSamples += s;
        }
        float dcOffset = sumSamples / samplesRead;

        float sumEnergy = 0.0f;
        float zeroCrossings = 0.0f;
        float prevSample = 0.0f;

        float sumBass = 0.0f, sumMid = 0.0f, sumTreble = 0.0f;
        static float lpBass = 0.0f, hpTreble = 0.0f;

        for (size_t i = 0; i < samplesRead; i++) {
          float s = ((float)(rawSamples[i] >> 8) / 8388608.0f) - dcOffset;
          sumEnergy += s * s;

          if (i > 0 && ((s >= 0.0f && prevSample < 0.0f) || (s < 0.0f && prevSample >= 0.0f))) {
            zeroCrossings += 1.0f;
          }
          prevSample = s;

          lpBass += (s - lpBass) * 0.12f;                  // ~300Hz LP
          hpTreble += (s - hpTreble) * 0.45f;              // ~2.5kHz HP
          float trebleS = s - hpTreble;
          float midS = s - lpBass - trebleS;

          sumBass += lpBass * lpBass;
          sumMid += midS * midS;
          sumTreble += trebleS * trebleS;
        }

        float rmsTotal = sqrtf(sumEnergy / samplesRead) * sensMult;
        float rmsBass = sqrtf(sumBass / samplesRead) * sensMult;
        float rmsMid = sqrtf(sumMid / samplesRead) * sensMult;
        float rmsTreble = sqrtf(sumTreble / samplesRead) * sensMult;

        // Dynamic AGC with Headroom Margin (prevents early saturation clipping)
        float currentPeak = rmsTotal * headroomMult;
        if (currentPeak > maxPeak) {
          maxPeak = currentPeak;
        } else {
          maxPeak = fmaxf(0.04f, maxPeak * 0.994f); // Slow decay
        }

        // Apply Universal Noise Floor Gate
        auto applyNoiseGate = [noiseGateCutoff](float val) -> float {
          if (val < noiseGateCutoff) return 0.0f;
          return constrain((val - noiseGateCutoff) / (1.0f - noiseGateCutoff), 0.0f, 1.0f);
        };

        float normTotal = applyNoiseGate(rmsTotal / maxPeak);
        float normBass = applyNoiseGate(rmsBass / maxPeak);
        float normMid = applyNoiseGate(rmsMid / maxPeak);
        float normTreble = applyNoiseGate(rmsTreble / maxPeak);

        // Perceptual Logarithmic (dB) volume curve
        float logAmp = (normTotal > 0.001f) ? (log10f(1.0f + 9.0f * normTotal)) : 0.0f;
        logAmp = constrain(logAmp, 0.0f, 1.0f);

        // Configurable Beat Detection Sensitivity
        bassAvg += (normBass - bassAvg) * 0.1f;
        float beatMultiplier = 1.10f + ((100.0f - cfg.beatSens) / 100.0f) * 0.80f; // 10..90 maps to 1.18x .. 1.82x
        bool isBeat = (normBass > (bassAvg * beatMultiplier)) && (normBass > 0.25f);

        // Zero-Crossing Pitch Frequency Calculation
        float estFreqHz = (zeroCrossings * 0.5f) * (config::kAudioSampleRate / (float)samplesRead);

        // Pitch Range Bounds (e.g. 120 Hz to 2400 Hz)
        float pitchLow = (float)cfg.pitchLowHz;
        float pitchHigh = (float)fmaxf((float)cfg.pitchLowHz + 200.0f, (float)cfg.pitchHighHz);

        float targetHue = smoothedHue;
        if (normTotal > 0.02f) {
          float freqNorm = constrain((estFreqHz - pitchLow) / (pitchHigh - pitchLow), 0.0f, 1.0f);
          targetHue = freqNorm * 0.75f; // Red -> Yellow -> Green -> Cyan -> Blue -> Purple
        }

        // Pitch Glide Temporal Smoothness
        float pitchAlpha = (float)cfg.pitchSmooth / 100.0f;
        pitchAlpha = constrain(pitchAlpha, 0.01f, 0.30f);
        smoothedHue += (targetHue - smoothedHue) * pitchAlpha;
        smoothedHue = fmodf(smoothedHue, 1.0f);
        if (smoothedHue < 0.0f) smoothedHue += 1.0f;

        if (g_bandsMutex && xSemaphoreTake(g_bandsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          g_currentBands.bass = normBass;
          g_currentBands.mid = normMid;
          g_currentBands.treble = normTreble;
          g_currentBands.totalAmp = normTotal;
          g_currentBands.logAmp = logAmp;
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

  xTaskCreate(audioTask, "audio_dsp_task", 4096, nullptr, 1, nullptr);
}

}  // namespace audio_dsp
