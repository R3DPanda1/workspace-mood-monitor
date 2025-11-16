/**
 * mmwave_sensor.h
 *
 * HMMD mmWave Sensor (S3KM1110) configuration
 * 24GHz mmWave radar for human presence detection
 */

#ifndef MMWAVE_SENSOR_H
#define MMWAVE_SENSOR_H

#include <Arduino.h>
#include <HardwareSerial.h>

// ==================== PIN DEFINITIONS ====================
// Optimal UART1 pins for ESP32-S3 (default UART1 assignment)
#define MMWAVE_TX      17   // ESP32 TX -> Sensor RX
#define MMWAVE_RX      18   // ESP32 RX -> Sensor TX
#define MMWAVE_OUT     5    // Sensor GPIO OUT pin (presence detection, active high)

// ==================== UART CONFIGURATION ====================
#define MMWAVE_UART_NUM     1        // UART1
#define MMWAVE_BAUD_RATE    115200   // Default baud rate for S3KM1110
#define MMWAVE_UART_BUFFER  1024     // RX/TX buffer size

// ==================== SENSOR SPECIFICATIONS ====================
// Detection range: 0.5m - 12m (configurable via UART)
// Power: 3.3V (3.0V - 3.6V)
// Communication: UART (115200 baud) + GPIO OUT
// Protocol: Hexadecimal, little-endian format

#endif // MMWAVE_SENSOR_H
