/**
 * audio_sensor.cpp
 *
 * INMP441 sound level measurement and FreeRTOS task (standalone)
 */

#include "audio_sensor.h"
#include "config.h"
#include "onem2m.h"
#include <math.h>

// ==================== GLOBAL STATE ====================
AudioSensorState audioState = {
  .currentLevel = 0.0,
  .lastReportedLevel = -1.0,
  .initialized = false,
  .mutex = NULL
};

static TaskHandle_t audioTaskHandle = NULL;

// ==================== INITIALIZATION ====================
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

// ==================== READING AUDIO LEVEL ====================
bool readAudioLevel(double& level) {
  if (!audioState.initialized) return false;

  int32_t i2s_data[I2S_READ_LEN];
  size_t bytes_read;

  if (i2s_read(I2S_NUM_0, &i2s_data, sizeof(i2s_data), &bytes_read, 100) != ESP_OK)
    return false;

  int samples = bytes_read / 4;
  double sum = 0;
  for (int i = 0; i < samples; i++) {
    int32_t sample = i2s_data[i] >> 8;
    sum += (double)sample * (double)sample;
  }

  double rms = sqrt(sum / samples);
  level = rms / 50;  // scale factor, adjust for your mic
  if (level > 1023) level = 1023;
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

// ==================== FREE RTOS TASK ====================
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
                // Update OneM2M
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
