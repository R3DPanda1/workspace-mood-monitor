#ifndef LED_ACTUATOR_H
#define LED_ACTUATOR_H

#include <Arduino.h>
#include <WebServer.h>

#define NEOPIXEL_PIN 38
#define NUMPIXELS 1
#define BRIGHTNESS 50
#define NOTIFICATION_PORT 8888

bool initLEDActuator();
bool startLEDActuatorTasks();
void setupLEDSubscriptions();

bool createLampDevice();
bool createBinarySwitch();
bool createColor();

extern WebServer* notificationServer;
extern String notificationURL;

#endif
