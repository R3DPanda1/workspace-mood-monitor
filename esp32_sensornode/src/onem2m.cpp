#include "onem2m.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClient.h>

OneM2MPaths onem2mPaths;

void OneM2MPaths::initialize(const char* host, int port, const char* cseName,
                             const char* aeName, const char* roomName, const char* deskName, const char* deviceName) {
    BASE_URL = String("http://") + host + ":" + String(port);
    CSE_PATH = "/" + String(cseName);
    AE_PATH = CSE_PATH + "/" + String(aeName);
    ROOM_PATH = AE_PATH + "/" + String(roomName);
    DESK_PATH = ROOM_PATH + "/" + String(deskName);
    DEVICE_PATH = DESK_PATH + "/" + String(deviceName);
}

String generateRequestId() {
    static unsigned long counter = 0;
    return String("req_") + String(counter++);
}

bool oneM2MRequest(const char* method, const String& path, const String& payload,
                   int resourceType, String& response, int& statusCode) {
    WiFiClient* client = new WiFiClient();
    if (!client) {
        statusCode = -1;
        return false;
    }

    HTTPClient http;
    String url = onem2mPaths.BASE_URL + path;
    url.trim();

    if (!http.begin(*client, url)) {
        delete client;
        statusCode = -1;
        return false;
    }

    http.setTimeout(5000);
    http.addHeader("X-M2M-Origin", ORIGINATOR);
    http.addHeader("X-M2M-RI", generateRequestId());
    http.addHeader("X-M2M-RVI", "3");
    http.addHeader("Accept", "application/json");

    if (resourceType > 0) {
        http.addHeader("Content-Type", "application/json;ty=" + String(resourceType));
    } else {
        http.addHeader("Content-Type", "application/json");
    }

    int httpCode = -1;
    if (strcmp(method, "GET") == 0) httpCode = http.GET();
    else if (strcmp(method, "POST") == 0) httpCode = http.POST(payload);
    else if (strcmp(method, "DELETE") == 0) httpCode = http.sendRequest("DELETE");
    else if (strcmp(method, "PUT") == 0) httpCode = http.PUT(payload);

    statusCode = httpCode;
    if (httpCode > 0) response = http.getString();

    http.end();
    client->stop();
    delete client;

    return (httpCode > 0);
}

bool oneM2MGet(const String& path, String& response, int& statusCode) {
    return oneM2MRequest("GET", path, "", 0, response, statusCode);
}

bool oneM2MPost(const String& path, const String& payload, int resourceType,
                String& response, int& statusCode) {
    return oneM2MRequest("POST", path, payload, resourceType, response, statusCode);
}

bool oneM2MDelete(const String& path, String& response, int& statusCode) {
    return oneM2MRequest("DELETE", path, "", 0, response, statusCode);
}

bool oneM2MPut(const String& path, const String& payload,
               String& response, int& statusCode) {
    return oneM2MRequest("PUT", path, payload, 0, response, statusCode);
}

bool waitForCSE(int maxAttempts) {
    Serial.print("Waiting for CSE");
    for (int i = 0; i < maxAttempts; i++) {
        String response;
        int statusCode;
        if (oneM2MGet(onem2mPaths.CSE_PATH, response, statusCode)) {
            if (statusCode == 200 || statusCode == 403) {
                Serial.println(" ready");
                return true;
            }
        }
        Serial.print(".");
        delay(2000);
    }
    Serial.println(" failed");
    return false;
}

bool createContainer(const char* containerName) {
    StaticJsonDocument<512> doc;
    JsonObject cnt = doc.createNestedObject("m2m:cnt");
    cnt["rn"] = containerName;
    JsonArray acpi = cnt.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    cnt["mbs"] = 10000;
    cnt["mni"] = 10;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;

    // Determine parent path based on container name
    String parentPath;
    if (strcmp(containerName, ROOM_CONTAINER) == 0) {
        parentPath = onem2mPaths.AE_PATH;
    } else {
        parentPath = onem2mPaths.ROOM_PATH;
    }

    oneM2MPost(parentPath, payload, ONEM2M_RT_CONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.printf("%s container ready\n", containerName);
        return true;
    }
    Serial.printf("%s container creation failed (%d)\n", containerName, statusCode);
    return false;
}

bool createLuxDevice() {
    // Create sensor with labels
    StaticJsonDocument<512> doc;
    JsonObject luxSensor = doc.createNestedObject("mio:luxSr");
    luxSensor["rn"] = LUX_DEVICE_NAME;
    luxSensor["cnd"] = "org.fhtwmio.common.moduleclass.mioLuxSensor";
    JsonArray acpi = luxSensor.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    JsonArray lbl = luxSensor.createNestedArray("lbl");
    lbl.add(String("room:") + ROOM_CONTAINER);
    lbl.add(String("desk:") + DESK_CONTAINER);
    lbl.add("sensor:lux");
    luxSensor["lux"] = 0.0;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    oneM2MPost(onem2mPaths.DESK_PATH, payload, ONEM2M_RT_FLEXCONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.println("Lux sensor ready");

        // Add announcement attributes (may fail if IN-CSE not connected)
        StaticJsonDocument<256> annDoc;
        JsonObject annSensor = annDoc.createNestedObject("mio:luxSr");
        JsonArray at = annSensor.createNestedArray("at");
        at.add("/id-cloud-in-cse");
        JsonArray aa = annSensor.createNestedArray("aa");
        aa.add("lux");

        String annPayload;
        serializeJson(annDoc, annPayload);

        String luxPath = onem2mPaths.DESK_PATH + "/" + String(LUX_DEVICE_NAME);
        oneM2MPut(luxPath, annPayload, response, statusCode);

        return true;
    }
    Serial.printf("Lux sensor creation failed (%d)\n", statusCode);
    return false;
}

bool updateLuxValue(float luxValue) {
    StaticJsonDocument<256> doc;
    JsonObject luxSensor = doc.createNestedObject("mio:luxSr");
    luxSensor["lux"] = luxValue;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    oneM2MPut(onem2mPaths.DEVICE_PATH, payload, response, statusCode);

    if (statusCode == 200 || statusCode == 204) {
        Serial.printf("Lux: %.1f lux\n", luxValue);
        return true;
    }
    return false;
}

bool createAudioDevice() {
    // Create sensor with labels
    StaticJsonDocument<512> doc;
    JsonObject audioSensor = doc.createNestedObject("cod:acoSr");
    audioSensor["rn"] = AUDIO_DEVICE_NAME;
    audioSensor["cnd"] = "org.onem2m.common.moduleclass.acousticSensor";
    JsonArray acpi = audioSensor.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    JsonArray lbl = audioSensor.createNestedArray("lbl");
    lbl.add(String("room:") + ROOM_CONTAINER);
    lbl.add(String("desk:") + DESK_CONTAINER);
    lbl.add("sensor:acoustic");
    audioSensor["louds"] = 0.0;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    oneM2MPost(onem2mPaths.DESK_PATH, payload, ONEM2M_RT_FLEXCONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.println("Audio sensor ready");

        // Add announcement attributes (may fail if IN-CSE not connected)
        StaticJsonDocument<256> annDoc;
        JsonObject annSensor = annDoc.createNestedObject("cod:acoSr");
        JsonArray at = annSensor.createNestedArray("at");
        at.add("/id-cloud-in-cse");
        JsonArray aa = annSensor.createNestedArray("aa");
        aa.add("louds");

        String annPayload;
        serializeJson(annDoc, annPayload);

        String audioPath = onem2mPaths.DESK_PATH + "/" + String(AUDIO_DEVICE_NAME);
        oneM2MPut(audioPath, annPayload, response, statusCode);

        return true;
    }
    Serial.printf("Audio sensor creation failed (%d)\n", statusCode);
    return false;
}

bool updateAudioValue(float loudness) {
    StaticJsonDocument<256> doc;
    JsonObject audioSensor = doc.createNestedObject("cod:acoSr");
    audioSensor["louds"] = loudness;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    String audioPath = onem2mPaths.DESK_PATH + "/" + String(AUDIO_DEVICE_NAME);
    oneM2MPut(audioPath, payload, response, statusCode);

    if (statusCode == 200 || statusCode == 204) {
        Serial.printf("Audio: %.1f\n", loudness);
        return true;
    }
    return false;
}

bool createOccupancyDevice() {
    // Create sensor with labels
    StaticJsonDocument<512> doc;
    JsonObject occSensor = doc.createNestedObject("mio:occSr");
    occSensor["rn"] = OCCUPANCY_DEVICE_NAME;
    occSensor["cnd"] = "org.fhtwmio.common.moduleclass.mioOccupancySensor";
    JsonArray acpi = occSensor.createNestedArray("acpi");
    acpi.add(String(CSE_NAME) + "/acpMoodMonitor");
    JsonArray lbl = occSensor.createNestedArray("lbl");
    lbl.add(String("room:") + ROOM_CONTAINER);
    lbl.add(String("desk:") + DESK_CONTAINER);
    lbl.add("sensor:occupancy");
    occSensor["occ"] = false;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    oneM2MPost(onem2mPaths.DESK_PATH, payload, ONEM2M_RT_FLEXCONTAINER, response, statusCode);

    if (statusCode == 201 || statusCode == 409) {
        Serial.println("Occupancy sensor ready");

        // Add announcement attributes (may fail if IN-CSE not connected)
        StaticJsonDocument<256> annDoc;
        JsonObject annSensor = annDoc.createNestedObject("mio:occSr");
        JsonArray at = annSensor.createNestedArray("at");
        at.add("/id-cloud-in-cse");
        JsonArray aa = annSensor.createNestedArray("aa");
        aa.add("occ");

        String annPayload;
        serializeJson(annDoc, annPayload);

        String occPath = onem2mPaths.DESK_PATH + "/" + String(OCCUPANCY_DEVICE_NAME);
        oneM2MPut(occPath, annPayload, response, statusCode);

        return true;
    }
    Serial.printf("Occupancy sensor creation failed (%d)\n", statusCode);
    return false;
}

bool updateOccupancyValue(bool occupied) {
    StaticJsonDocument<256> doc;
    JsonObject occSensor = doc.createNestedObject("mio:occSr");
    occSensor["occ"] = occupied;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    String occPath = onem2mPaths.DESK_PATH + "/" + String(OCCUPANCY_DEVICE_NAME);
    oneM2MPut(occPath, payload, response, statusCode);

    bool success = (statusCode == 200 || statusCode == 204);

    // Sync occupancy to lamp if enabled
    #if SYNC_OCCUPANCY_TO_LAMP
    if (success) {
        updateLampSwitch(occupied);
    }
    #endif

    return success;
}

bool updateLampSwitch(bool on) {
    StaticJsonDocument<256> doc;
    JsonObject binSwitch = doc.createNestedObject("cod:binSh");
    binSwitch["state"] = on;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode;
    String switchPath = onem2mPaths.DESK_PATH + "/lamp/binarySwitch";
    oneM2MPut(switchPath, payload, response, statusCode);

    return (statusCode == 200 || statusCode == 204);
}
