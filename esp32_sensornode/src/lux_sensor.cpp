/**
 * lux_sensor.cpp
 *
 * VEML7700 lux sensor implementation with FreeRTOS task
 */

#include "lux_sensor.h"
#include "onem2m.h"
#include "config.h"
#include <Wire.h>

// ==================== GLOBAL STATE ====================

LuxSensorState luxState = {
    .currentLux = 0.0,
    .lastReportedLux = -1.0,
    .initialized = false,
    .mutex = NULL
};

// Local sensor instance
static Adafruit_VEML7700 veml;
static TaskHandle_t luxTaskHandle = NULL;

// ==================== SENSOR INITIALIZATION ====================

bool initLuxSensor() {
    Serial.println("\n=== Initializing VEML7700 ===");

    // Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // Initialize sensor
    if (!veml.begin()) {
        Serial.println("ERROR: Failed to find VEML7700 sensor!");
        return false;
    }

    // Create mutex for thread-safe access
    luxState.mutex = xSemaphoreCreateMutex();
    if (luxState.mutex == NULL) {
        Serial.println("ERROR: Failed to create lux mutex");
        return false;
    }

    luxState.initialized = true;
    Serial.println("VEML7700 initialized successfully");

    return true;
}

// ==================== SENSOR READING ====================

bool readLuxValue(float& luxValue) {
    if (!luxState.initialized) {
        return false;
    }

    luxValue = veml.readLux();
    return true;
}

float getLastReportedLux() {
    float value;
    xSemaphoreTake(luxState.mutex, portMAX_DELAY);
    value = luxState.lastReportedLux;
    xSemaphoreGive(luxState.mutex);
    return value;
}

void setLastReportedLux(float luxValue) {
    xSemaphoreTake(luxState.mutex, portMAX_DELAY);
    luxState.lastReportedLux = luxValue;
    xSemaphoreGive(luxState.mutex);
}

// ==================== FREERTOS TASK ====================

void LuxSensorTask(void* pvParameters) {
    Serial.println("LuxSensorTask started");

    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t updateInterval = pdMS_TO_TICKS(LUX_UPDATE_INTERVAL);

    while (true) {
        float currentLux;

        // Read sensor
        if (readLuxValue(currentLux)) {
            // Update current value (thread-safe)
            xSemaphoreTake(luxState.mutex, portMAX_DELAY);
            luxState.currentLux = currentLux;
            xSemaphoreGive(luxState.mutex);

            float lastReported = getLastReportedLux();

            // Check if change is significant enough to report
            bool shouldReport = (lastReported < 0) ||
                              (abs(currentLux - lastReported) >= LUX_THRESHOLD);

            if (shouldReport) {
                Serial.println("Lux reading: " + String(currentLux) + " lux");

                // Update OneM2M
                if (updateLuxValue(currentLux)) {
                    setLastReportedLux(currentLux);
                }
            }
        } else {
            Serial.println("ERROR: Failed to read lux sensor");
        }

        // Wait for next update interval
        vTaskDelayUntil(&lastWakeTime, updateInterval);
    }
}

// ==================== TASK MANAGEMENT ====================

bool startLuxSensorTask() {
    BaseType_t result = xTaskCreatePinnedToCore(
        LuxSensorTask,
        "LuxSensor",
        LUX_TASK_STACK_SIZE,
        NULL,
        LUX_TASK_PRIORITY,
        &luxTaskHandle,
        0  // Core 0
    );

    if (result != pdPASS) {
        Serial.println("ERROR: Failed to create LuxSensorTask");
        return false;
    }

    Serial.println("LuxSensorTask created successfully");
    return true;
}
