#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "audio_sensor.h"
#include "occupancy_sensor.h"
#include "onem2m.h"
#include "lux_sensor.h"
#include "led_actuator.h"

bool connectWiFi() {
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" failed");
        return false;
    }

    Serial.printf(" connected\nIP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== VibeTribe Mood Monitor ===");
    Serial.println("2025 International oneM2M Hackathon\n");

    if (!connectWiFi()) {
        Serial.println("WiFi failed - halting");
        while (1) delay(1000);
    }

    onem2mPaths.initialize(CSE_HOST, CSE_PORT, CSE_NAME, AE_NAME, ROOM_CONTAINER, DESK_CONTAINER, LUX_DEVICE_NAME);

    if (!waitForCSE()) {
        Serial.println("CSE unavailable - halting");
        while (1) delay(1000);
    }

    createContainer(ROOM_CONTAINER);
    delay(500);
    createContainer(DESK_CONTAINER);
    delay(500);

    createLuxDevice();
    delay(500);
    createAudioDevice();
    delay(500);
    createOccupancyDevice();
    delay(500);
    createLampDevice();
    delay(500);
    createBinarySwitch();
    delay(500);
    createColor();
    delay(500);

    if (!initLuxSensor() || !startLuxSensorTask()) {
        Serial.println("Lux sensor failed - halting");
        while (1) delay(1000);
    }

    if (!initAudioSensor() || !startAudioSensorTask()) {
        Serial.println("Audio sensor failed - halting");
        while (1) delay(1000);
    }

    if (!initOccupancySensor() || !startOccupancySensorTask()) {
        Serial.println("Occupancy sensor failed - halting");
        while (1) delay(1000);
    }

    if (!initLEDActuator() || !startLEDActuatorTasks()) {
        Serial.println("LED actuator failed - halting");
        while (1) delay(1000);
    }

    delay(2000);
    setupLEDSubscriptions();

    Serial.println("\nSystem ready\n");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost - reconnecting");
        WiFi.reconnect();
        delay(5000);
    }
    delay(1000);
}
