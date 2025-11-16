#ifndef OCCUPANCY_SENSOR_H
#define OCCUPANCY_SENSOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ==================== PIN DEFINITIONS ====================
// mmWave sensor pins for ESP32-S3
#define OCCUPANCY_OT2_PIN   1    // OT2 detection output (GPIO)
#define RADAR_RX_PIN        18   // ESP32 RX <- Sensor TX
#define RADAR_TX_PIN        17   // ESP32 TX -> Sensor RX

// ==================== FUNCTIONS ====================
bool initOccupancySensor();
bool startOccupancySensorTask();
bool getOccupancyDetected();

#endif // OCCUPANCY_SENSOR_H
