#include "occupancy_sensor.h"
#include "config.h"
#include "onem2m.h"
#include <HardwareSerial.h>

static HardwareSerial radarSerial(1);
static TaskHandle_t occupancyTaskHandle = NULL;
static SemaphoreHandle_t occupancyMutex = NULL;
static volatile bool isOccupied = false;
static bool lastReportedState = false;

void sendHexData(String hexString) {
    int len = hexString.length();
    byte bytes[len / 2];
    for (int i = 0; i < len; i += 2) {
        bytes[i / 2] = strtoul(hexString.substring(i, i + 2).c_str(), NULL, 16);
    }
    radarSerial.write(bytes, sizeof(bytes));
}

bool initOccupancySensor() {
    occupancyMutex = xSemaphoreCreateMutex();
    if (!occupancyMutex) return false;

    radarSerial.begin(115200, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
    pinMode(OCCUPANCY_OT2_PIN, INPUT);
    delay(500);

    sendHexData("FDFCFBFA0800120000006400000004030201");
    delay(100);

    Serial.println("Occupancy sensor ready");
    return true;
}

bool getOccupancyDetected() {
    if (!occupancyMutex) return false;
    bool occupied;
    xSemaphoreTake(occupancyMutex, portMAX_DELAY);
    occupied = isOccupied;
    xSemaphoreGive(occupancyMutex);
    return occupied;
}

void OccupancySensorTask(void* pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(2000));

    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(OCCUPANCY_UPDATE_INTERVAL);
    bool firstReport = true;
    bool lastLocalState = false;

    while (true) {
        bool pinState = digitalRead(OCCUPANCY_OT2_PIN);

        if (pinState != lastLocalState) {
            lastLocalState = pinState;
            xSemaphoreTake(occupancyMutex, portMAX_DELAY);
            isOccupied = pinState;
            xSemaphoreGive(occupancyMutex);
        }

        bool currentState = getOccupancyDetected();
        bool shouldReport = firstReport || (currentState != lastReportedState);

        if (shouldReport) {
            if (updateOccupancyValue(currentState)) {
                lastReportedState = currentState;
                Serial.printf("Occupancy: %s\n", currentState ? "OCCUPIED" : "EMPTY");
            }
            firstReport = false;
        }

        vTaskDelayUntil(&lastWake, interval);
    }
}

bool startOccupancySensorTask() {
    BaseType_t result = xTaskCreatePinnedToCore(
        OccupancySensorTask, "OccupancySensor",
        4096, NULL, 1, &occupancyTaskHandle, 1
    );
    return (result == pdPASS);
}
