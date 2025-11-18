/**
 * audio_sensor.cpp
 *
 * INMP441 MEMS microphone - Sound level measurement in dB SPL
 * Reads I2S audio data and converts to decibels (Sound Pressure Level)
 */

#include "audio_sensor.h"
#include "config.h"
#include "onem2m.h"
#include <math.h>

// Global state
AudioSensorState audioState = {
  .currentLevel = 0.0,
  .lastReportedLevel = -1.0,
  .initialized = false,
  .mutex = NULL
};

static TaskHandle_t audioTaskHandle = NULL;

// Initialize INMP441 I2S microphone
bool initAudioSensor() {
  Serial.println("\n=== Initializing INMP441 Audio Sensor ===");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 32,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("ERROR: I2S driver install failed!");
    return false;
  }

  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
    Serial.println("ERROR: I2S pin config failed!");
    return false;
  }

  audioState.mutex = xSemaphoreCreateMutex();
  if (!audioState.mutex) {
    Serial.println("ERROR: Failed to create audio mutex");
    return false;
  }

  audioState.initialized = true;
  Serial.println("INMP441 initialized successfully");
  return true;
}

// Read audio level and convert to dB SPL
bool readAudioLevel(double& level) {
  if (!audioState.initialized) {
    return false;
  }

  int32_t i2s_data[I2S_READ_LEN];
  size_t bytes_read;

  if (i2s_read(I2S_NUM_0, &i2s_data, sizeof(i2s_data), &bytes_read, 100) != ESP_OK) {
    return false;
  }

  int samples = bytes_read / 4;
  double sum = 0.0;

  // Calculate RMS from I2S samples
  for (int i = 0; i < samples; i++) {
    // INMP441 outputs 24-bit data in 32-bit words (left-aligned)
    // Shift right by 8 to extract the 24-bit signed value
    int32_t sample = i2s_data[i] >> 8;
    sum += (double)sample * (double)sample;
  }

  double rms = sqrt(sum / samples);

  // Convert RMS to dB SPL (Sound Pressure Level)
  //
  // From INMP441 datasheet (https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf):
  //   - Sensitivity: -26 dBFS at 94 dB SPL (1 kHz reference)
  //   - 24-bit output format
  //
  // Calculation:
  //   - Full scale = 2^23 = 8,388,608 (max amplitude for 24-bit signed)
  //   - dBFS = 20 * log10(rms / full_scale)
  //   - At -26 dBFS → 94 dB SPL, therefore at 0 dBFS → 94 - (-26) = 120 dB SPL
  //   - Final formula: dB_SPL = 20 * log10(rms / full_scale) + 120

  const double FULL_SCALE = 8388608.0;  // 2^23
  const double DB_OFFSET = 120.0;       // Derived from -26 dBFS = 94 dB SPL

  if (rms > 0) {
    level = 20.0 * log10(rms / FULL_SCALE) + DB_OFFSET;
  } else {
    level = 0.0;
  }

  return true;
}

float getLastReportedAudioLevel() {
  xSemaphoreTake(audioState.mutex, portMAX_DELAY);
  float val = audioState.lastReportedLevel;
  xSemaphoreGive(audioState.mutex);
  return val;
}

void setLastReportedAudioLevel(float level) {
  xSemaphoreTake(audioState.mutex, portMAX_DELAY);
  audioState.lastReportedLevel = level;
  xSemaphoreGive(audioState.mutex);
}

// FreeRTOS task for periodic audio monitoring
void AudioSensorTask(void* pvParameters) {
  Serial.println("AudioSensorTask started");
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(AUDIO_UPDATE_INTERVAL);

  while (true) {
    double currentLevel;
    if (readAudioLevel(currentLevel)) {
      xSemaphoreTake(audioState.mutex, portMAX_DELAY);
      audioState.currentLevel = currentLevel;
      xSemaphoreGive(audioState.mutex);

      double last = getLastReportedAudioLevel();
      bool shouldReport = (last < 0) || (fabs(currentLevel - last) >= AUDIO_THRESHOLD);

      if (shouldReport) {
        if (updateAudioValue(currentLevel)) {
          setLastReportedAudioLevel(currentLevel);
        }
      }
    } else {
      Serial.println("ERROR: Failed to read audio sensor");
    }

    vTaskDelayUntil(&lastWake, interval);
  }
}

bool startAudioSensorTask() {
  BaseType_t result = xTaskCreatePinnedToCore(
    AudioSensorTask, "AudioSensor", 4096, NULL, 1, &audioTaskHandle, 1);

  if (result != pdPASS) {
    Serial.println("ERROR: Failed to create AudioSensorTask");
    return false;
  }

  Serial.println("AudioSensorTask created successfully");
  return true;
}
