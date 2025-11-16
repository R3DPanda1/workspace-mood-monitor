/**
 * audio_sensor.h
 * 
 * INMP441 audio level sensor header
 */

#ifndef AUDIO_SENSOR_H
#define AUDIO_SENSOR_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ==================== AUDIO SENSOR CONFIGURATION ====================
// Optimal pins for ESP32-S3 I2S (HSPI group, no conflicts)
#define I2S_SCK_PIN  12   // BCLK (Serial Clock)
#define I2S_WS_PIN   11   // LRCLK (Word Select)
#define I2S_SD_PIN   10   // DOUT (Serial Data)

#define SAMPLE_RATE  44100
#define I2S_READ_LEN 128
#define AVG_COUNT    8

// Note: AUDIO_UPDATE_INTERVAL and AUDIO_THRESHOLD are now defined in config.h/cpp


// ==================== STATE STRUCT ====================
struct AudioSensorState {
  double currentLevel;
  double lastReportedLevel;
  bool initialized;
  SemaphoreHandle_t mutex;
};

extern AudioSensorState audioState;

// ==================== FUNCTIONS ====================
bool initAudioSensor();
bool startAudioSensorTask();
float getLastReportedAudioLevel();
void setLastReportedAudioLevel(float level);

#endif
