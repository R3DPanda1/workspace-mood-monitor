#ifndef CONFIG_H
#define CONFIG_H

// WiFi
#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"

// OneM2M CSE
#define CSE_HOST "raspberry_ip"
#define CSE_PORT 8081
#define CSE_NAME "room-mn-cse"
#define ORIGINATOR "CMoodMonitor"
#define AE_NAME "moodMonitorAE"
#define ROOM_CONTAINER "Room01"
#define DESK_CONTAINER "Desk01"

// Device names
#define LUX_DEVICE_NAME "luxSensor"
#define AUDIO_DEVICE_NAME "acousticSensor"
#define OCCUPANCY_DEVICE_NAME "occupancySensor"

// Update intervals (ms)
#define LUX_UPDATE_INTERVAL 10000
#define AUDIO_UPDATE_INTERVAL 10000
#define OCCUPANCY_UPDATE_INTERVAL 10000

// Thresholds
#define LUX_THRESHOLD 1.0f
#define AUDIO_THRESHOLD 5.0f

// Occupancy automation
#define SYNC_OCCUPANCY_TO_LAMP true  // Set to false to disable automatic lamp control

// I2C pins (VEML7700)
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// FreeRTOS
#define LUX_TASK_STACK_SIZE 4096
#define LUX_TASK_PRIORITY 1

#endif
