#include "led_actuator.h"
#include "config.h"
#include "onem2m.h"
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
WebServer* notificationServer = nullptr;
String notificationURL = "";

static SemaphoreHandle_t ledMutex = NULL;
static TaskHandle_t neopixelTaskHandle = NULL;
static TaskHandle_t notificationTaskHandle = NULL;

static bool lampOn = false;
static uint8_t redValue = 0;
static uint8_t greenValue = 0;
static uint8_t blueValue = 0;

void setLEDState(bool on, uint8_t r, uint8_t g, uint8_t b) {
    if (ledMutex) {
        xSemaphoreTake(ledMutex, portMAX_DELAY);
        lampOn = on;
        redValue = r;
        greenValue = g;
        blueValue = b;
        xSemaphoreGive(ledMutex);
    }
}

void getLEDState(bool& on, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (ledMutex) {
        xSemaphoreTake(ledMutex, portMAX_DELAY);
        on = lampOn;
        r = redValue;
        g = greenValue;
        b = blueValue;
        xSemaphoreGive(ledMutex);
    }
}

void taskNeoPixelUpdate(void* pvParameters) {
    // Wait for initialization to complete
    vTaskDelay(pdMS_TO_TICKS(500));

    while (true) {
        if (!ledMutex) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        bool on;
        uint8_t r, g, b;
        getLEDState(on, r, g, b);

        if (on) {
            pixels.setPixelColor(0, pixels.Color(r, g, b));
        } else {
            pixels.setPixelColor(0, pixels.Color(0, 0, 0));
        }
        pixels.show();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void handleNotification() {
    if (!notificationServer) return;

    String body = notificationServer->arg("plain");

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        notificationServer->send(400, "text/plain", "Invalid JSON");
        return;
    }

    if (doc.containsKey("m2m:sgn")) {
        JsonObject sgn = doc["m2m:sgn"];

        if (sgn.containsKey("vrq") && sgn["vrq"] == true) {
            notificationServer->send(200, "text/plain", "OK");
            Serial.println("Subscription verified");
            return;
        }

        if (sgn.containsKey("nev") && sgn["nev"].containsKey("rep")) {
            JsonObject rep = sgn["nev"]["rep"];

            if (rep.containsKey("cod:binSh")) {
                bool powerState = rep["cod:binSh"]["state"];
                bool currentOn;
                uint8_t r, g, b;
                getLEDState(currentOn, r, g, b);
                setLEDState(powerState, r, g, b);
                Serial.printf("LED power: %s\n", powerState ? "ON" : "OFF");
            }

            if (rep.containsKey("cod:color")) {
                int red = rep["cod:color"]["red"];
                int green = rep["cod:color"]["green"];
                int blue = rep["cod:color"]["blue"];
                bool currentOn;
                uint8_t oldR, oldG, oldB;
                getLEDState(currentOn, oldR, oldG, oldB);
                setLEDState(currentOn, red, green, blue);
                Serial.printf("LED color: R%d G%d B%d\n", red, green, blue);
            }
        }
    }

    notificationServer->send(200, "text/plain", "OK");
}

void taskNotificationServer(void* pvParameters) {
    notificationServer = new WebServer(NOTIFICATION_PORT);
    notificationServer->on("/", []() {
        notificationServer->send(200, "text/plain", "ESP32-S3 Lamp Notification Server");
    });
    notificationServer->on("/notify", HTTP_POST, handleNotification);
    notificationServer->begin();
    Serial.printf("Notification server started on port %d\n", NOTIFICATION_PORT);

    while (true) {
        notificationServer->handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool createLampDevice() {
    StaticJsonDocument<512> doc;
    JsonObject lamp = doc.createNestedObject("cod:devLt");
    lamp["rn"] = "lamp";
    lamp["cnd"] = "org.onem2m.common.device.deviceLight";
    JsonArray acpi = lamp.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    JsonArray lbl = lamp.createNestedArray("lbl");
    lbl.add(String("room:") + ROOM_CONTAINER);
    lbl.add(String("desk:") + DESK_CONTAINER);
    lbl.add("actuator:lamp");

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    oneM2MPost(onem2mPaths.DESK_PATH, payload, ONEM2M_RT_FLEXCONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.println("Lamp device ready");
        return true;
    }
    Serial.printf("Lamp device creation failed (%d)\n", statusCode);
    return false;
}

bool createBinarySwitch() {
    StaticJsonDocument<512> doc;
    JsonObject binSwitch = doc.createNestedObject("cod:binSh");
    binSwitch["rn"] = "binarySwitch";
    binSwitch["cnd"] = "org.onem2m.common.moduleclass.binarySwitch";
    JsonArray acpi = binSwitch.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    binSwitch["state"] = false;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    String lampPath = onem2mPaths.DESK_PATH + "/lamp";
    oneM2MPost(lampPath, payload, ONEM2M_RT_FLEXCONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.println("Binary switch ready");

        // Initialize to OFF state
        StaticJsonDocument<256> initDoc;
        JsonObject initSwitch = initDoc.createNestedObject("cod:binSh");
        initSwitch["state"] = false;

        String initPayload;
        serializeJson(initDoc, initPayload);

        String switchPath = onem2mPaths.DESK_PATH + "/lamp/binarySwitch";
        oneM2MPut(switchPath, initPayload, response, statusCode);

        return true;
    }
    Serial.printf("Binary switch creation failed (%d)\n", statusCode);
    Serial.printf("Response: %s\n", response.c_str());
    return false;
}

bool createColor() {
    StaticJsonDocument<512> doc;
    JsonObject color = doc.createNestedObject("cod:color");
    color["rn"] = "color";
    color["cnd"] = "org.onem2m.common.moduleclass.colour";
    JsonArray acpi = color.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    color["red"] = 0;
    color["green"] = 0;
    color["blue"] = 0;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    String lampPath = onem2mPaths.DESK_PATH + "/lamp";
    oneM2MPost(lampPath, payload, ONEM2M_RT_FLEXCONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.println("Color module ready");

        // Initialize to no color (0,0,0)
        StaticJsonDocument<256> initDoc;
        JsonObject initColor = initDoc.createNestedObject("cod:color");
        initColor["red"] = 0;
        initColor["green"] = 0;
        initColor["blue"] = 0;

        String initPayload;
        serializeJson(initDoc, initPayload);

        String colorPath = onem2mPaths.DESK_PATH + "/lamp/color";
        oneM2MPut(colorPath, initPayload, response, statusCode);

        return true;
    }
    Serial.printf("Color module creation failed (%d)\n", statusCode);
    Serial.printf("Response: %s\n", response.c_str());
    return false;
}

bool createSubscription(const String& resourcePath, const String& subscriptionName) {
    StaticJsonDocument<1024> doc;
    JsonObject sub = doc.createNestedObject("m2m:sub");
    sub["rn"] = subscriptionName;

    JsonArray nu = sub.createNestedArray("nu");
    nu.add(notificationURL + "/notify");

    JsonObject enc = sub.createNestedObject("enc");
    JsonArray net = enc.createNestedArray("net");
    net.add(1);  // Create
    net.add(2);  // Delete
    net.add(3);  // Update
    net.add(4);  // Delete direct child

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    oneM2MPost(resourcePath, payload, ONEM2M_RT_SUBSCRIPTION, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.printf("Subscription '%s' created\n", subscriptionName.c_str());
        return true;
    }
    Serial.printf("Subscription '%s' failed (%d)\n", subscriptionName.c_str(), statusCode);
    return false;
}

void setupLEDSubscriptions() {
    notificationURL = "http://" + WiFi.localIP().toString() + ":" + String(NOTIFICATION_PORT);
    Serial.printf("Notification URL: %s\n", notificationURL.c_str());

    delay(1000);

    String switchPath = onem2mPaths.DESK_PATH + "/lamp/binarySwitch";
    createSubscription(switchPath, "subLampSwitch");
    delay(500);

    String colorPath = onem2mPaths.DESK_PATH + "/lamp/color";
    createSubscription(colorPath, "subLampColor");
    delay(500);
}

bool initLEDActuator() {
    ledMutex = xSemaphoreCreateMutex();
    if (!ledMutex) return false;

    pixels.begin();
    pixels.setBrightness(BRIGHTNESS);
    pixels.clear();
    pixels.show();

    // Initialize to OFF with no color
    setLEDState(false, 0, 0, 0);

    Serial.println("LED actuator ready");
    return true;
}

bool startLEDActuatorTasks() {
    BaseType_t result1 = xTaskCreatePinnedToCore(
        taskNeoPixelUpdate, "NeoPixelUpdate",
        4096, NULL, 1, &neopixelTaskHandle, 1
    );

    BaseType_t result2 = xTaskCreatePinnedToCore(
        taskNotificationServer, "NotificationServer",
        8192, NULL, 1, &notificationTaskHandle, 1
    );

    return (result1 == pdPASS && result2 == pdPASS);
}
