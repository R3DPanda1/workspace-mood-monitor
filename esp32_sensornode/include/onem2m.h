/**
 * onem2m.h
 *
 * OneM2M communication utilities for ESP32
 * Handles HTTP requests to OneM2M CSE and resource management
 */

#ifndef ONEM2M_H
#define ONEM2M_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ==================== ONEM2M RESOURCE TYPES ====================

#define ONEM2M_RT_CONTAINER 3
#define ONEM2M_RT_FLEXCONTAINER 28
#define ONEM2M_RT_SUBSCRIPTION 23

// ==================== ONEM2M PATHS ====================

class OneM2MPaths {
public:
    String BASE_URL;
    String CSE_PATH;
    String AE_PATH;
    String ROOM_PATH;
    String DESK_PATH;
    String DEVICE_PATH;

    void initialize(const char* host, int port, const char* cseName,
                   const char* aeName, const char* roomName, const char* deskName, const char* deviceName);
};

// Global instance
extern OneM2MPaths onem2mPaths;

// ==================== ONEM2M HTTP FUNCTIONS ====================

/**
 * Generate unique request ID for OneM2M requests
 */
String generateRequestId();

/**
 * Perform a generic OneM2M HTTP request
 * @param method HTTP method (GET, POST, DELETE, PUT)
 * @param path Resource path (relative to BASE_URL)
 * @param payload JSON payload (for POST/PUT)
 * @param resourceType OneM2M resource type (ty parameter)
 * @param response Output parameter for response body
 * @param statusCode Output parameter for HTTP status code
 * @return true if request succeeded (HTTP response received)
 */
bool oneM2MRequest(const char* method, const String& path, const String& payload,
                   int resourceType, String& response, int& statusCode);

/**
 * Perform OneM2M GET request
 */
bool oneM2MGet(const String& path, String& response, int& statusCode);

/**
 * Perform OneM2M POST request
 */
bool oneM2MPost(const String& path, const String& payload, int resourceType,
                String& response, int& statusCode);

/**
 * Perform OneM2M DELETE request
 */
bool oneM2MDelete(const String& path, String& response, int& statusCode);

/**
 * Perform OneM2M PUT request (for updating existing resources)
 */
bool oneM2MPut(const String& path, const String& payload,
               String& response, int& statusCode);

// ==================== CSE INITIALIZATION ====================

/**
 * Wait for CSE to be available
 * @param maxAttempts Maximum number of connection attempts
 * @return true if CSE is reachable
 */
bool waitForCSE(int maxAttempts = 30);

/**
 * Create a container (Room or Desk)
 * @param containerName Name of the container to create
 * @return true if created successfully or already exists
 */
bool createContainer(const char* containerName);

/**
 * Create the mioDeviceLux FlexContainer
 * @return true if created successfully or already exists
 */
bool createLuxDevice();

/**
 * Update lux value in the FlexContainer
 * @param luxValue Current lux reading
 * @return true if update succeeded
 */
bool updateLuxValue(float luxValue);

/**
 * Create the acousticSensor FlexContainer (OneM2M standard moduleclass)
 * @return true if created successfully or already exists
 */
bool createAudioDevice();

/**
 * Update audio loudness value in the FlexContainer
 * @param loudness Current loudness level
 * @return true if update succeeded
 */
bool updateAudioValue(float loudness);

/**
 * Create the occupancySensor FlexContainer (custom mio moduleclass)
 * @return true if created successfully or already exists
 */
bool createOccupancyDevice();

/**
 * Update occupancy value in the FlexContainer
 * @param occupied Current occupancy state
 * @return true if update succeeded
 */
bool updateOccupancyValue(bool occupied);

/**
 * Update lamp binary switch state
 * @param on Lamp power state
 * @return true if update succeeded
 */
bool updateLampSwitch(bool on);

#endif // ONEM2M_H
