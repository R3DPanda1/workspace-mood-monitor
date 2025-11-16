/**
 * lux_sensor.h
 *
 * VEML7700 lux sensor management with FreeRTOS task
 */

#ifndef LUX_SENSOR_H
#define LUX_SENSOR_H

#include <Arduino.h>
#include <Adafruit_VEML7700.h>

// ==================== LUX SENSOR STATE ====================

struct LuxSensorState {
    float currentLux;
    float lastReportedLux;
    bool initialized;
    SemaphoreHandle_t mutex;
};

// Global lux sensor state
extern LuxSensorState luxState;

// ==================== LUX SENSOR FUNCTIONS ====================

/**
 * Initialize the VEML7700 sensor
 * @return true if initialization succeeded
 */
bool initLuxSensor();

/**
 * Read current lux value from sensor
 * @param luxValue Output parameter for lux reading
 * @return true if read succeeded
 */
bool readLuxValue(float& luxValue);

/**
 * Get the last reported lux value (thread-safe)
 * @return Last lux value reported to OneM2M
 */
float getLastReportedLux();

/**
 * Update the last reported lux value (thread-safe)
 * @param luxValue New lux value to store
 */
void setLastReportedLux(float luxValue);

// ==================== FREERTOS TASK ====================

/**
 * FreeRTOS task for reading lux sensor and updating OneM2M
 * @param pvParameters Task parameters (unused)
 */
void LuxSensorTask(void* pvParameters);

/**
 * Start the lux sensor FreeRTOS task
 * @return true if task created successfully
 */
bool startLuxSensorTask();

#endif // LUX_SENSOR_H
