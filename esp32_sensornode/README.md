# VibeTribe Mood Monitor - Sensor Node

**2025 International oneM2M Hackathon**

ESP32-S3 sensor node that monitors ambient environment (light, audio, occupancy) and provides LED feedback. Connects to the Application Entity (AE) in the Middle Node Common Services Entity (MN-CSE) via oneM2M protocol.

## Project Overview

The Mood Monitor system consists of:
- **Sensor Node** (this device): ESP32-S3 with environmental sensors and LED actuator
- **MN-CSE**: Middle Node Common Services Entity hosting the application
- **AE (moodMonitorAE)**: Application Entity managing room data and automation logic

This sensor node creates oneM2M FlexContainer resources under the AE and uses subscriptions for instant LED control without polling.

## Hardware

### Components
- **ESP32-S3-DevKitC-1** - Main microcontroller
- **VEML7700** - Ambient light sensor (I2C)
- **INMP441** - Digital MEMS microphone (I2S)
- **S3KM1110** - mmWave occupancy sensor (UART + GPIO)
- **NeoPixel WS2812** - RGB LED indicator (built-in on GPIO 38)

### Pin Configuration

| Component | Function | GPIO |
|-----------|----------|------|
| VEML7700 | I2C SDA | 8 |
| VEML7700 | I2C SCL | 9 |
| INMP441 | I2S SCK (BCLK) | 12 |
| INMP441 | I2S WS (LRCLK) | 11 |
| INMP441 | I2S SD (DOUT) | 10 |
| mmWave | UART TX | 17 |
| mmWave | UART RX | 18 |
| mmWave | OT2 (Detection) | 1 |
| NeoPixel | DIN | 38 |

## Features

- **Multi-threaded FreeRTOS architecture** - Concurrent sensor and actuator operation
- **oneM2M standard compliance** - FlexContainer resources with proper moduleclasses
- **Subscription-based LED control** - Instant response (<100ms) via HTTP notifications
- **Efficient sensor updates** - 10-second polling with threshold-based reporting
- **Thread-safe state management** - Mutex-protected shared data
- **Automatic WiFi reconnection** - Robust network handling
- **HTTP notification server** - Receives oneM2M callbacks on port 8888

## OneM2M Resource Structure

```
MN-CSE (room-mn-cse)
└── moodMonitorAE
    └── Room01
        ├── luxSensor (mio:luxSr)
        │   └── lux: float
        ├── acousticSensor (cod:acoSr)
        │   └── louds: float
        ├── occupancySensor (mio:occSr)
        │   └── occ: boolean
        └── lamp (cod:devLt)
            ├── binarySwitch (cod:binSh)
            │   └── state: boolean
            └── color (cod:color)
                ├── red: int (0-255)
                ├── green: int (0-255)
                └── blue: int (0-255)
```

## Configuration

Edit `include/config.h` before building:

```cpp
// WiFi credentials
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"

// OneM2M CSE connection
#define CSE_HOST "192.168.x.x"
#define CSE_PORT 8081
#define CSE_NAME "room-mn-cse"
#define AE_NAME "moodMonitorAE"
#define ROOM_CONTAINER "Room01"
```

## Build and Upload

Using PlatformIO:

```bash
# Build the project
pio run

# Upload to ESP32
pio run -t upload

# Monitor serial output
pio device monitor
```

## Serial Output Example

```
=== VibeTribe Mood Monitor ===
2025 International oneM2M Hackathon

Connecting to YourNetwork... connected
IP: 192.168.1.100
Waiting for CSE... ready
Lux sensor ready
Audio sensor ready
Occupancy sensor ready
Lamp device ready
Binary switch ready
Color module ready
LED actuator ready
Notification server started on port 8888
Notification URL: http://192.168.1.100:8888/notify
Subscription 'subLampSwitch' created
Subscription 'subLampColor' created

System ready

Lux: 25.3 lux
Audio: 48.9
Occupancy: EMPTY
LED power: ON
LED color: R255 G200 B100
```

## Architecture

### File Structure

```
ESP32-SensorNode/
├── include/
│   ├── config.h              # System configuration
│   ├── onem2m.h              # oneM2M protocol functions
│   ├── lux_sensor.h          # VEML7700 interface
│   ├── audio_sensor.h        # INMP441 interface
│   ├── occupancy_sensor.h    # S3KM1110 interface
│   └── led_actuator.h        # NeoPixel + subscription handler
├── src/
│   ├── main.cpp              # System initialization
│   ├── onem2m.cpp            # oneM2M HTTP requests
│   ├── lux_sensor.cpp        # Light sensor driver
│   ├── audio_sensor.cpp      # Microphone driver
│   ├── occupancy_sensor.cpp  # mmWave sensor driver
│   └── led_actuator.cpp      # LED controller + notification server
└── platformio.ini            # Build configuration
```

### FreeRTOS Tasks

Each component runs in its own task on separate CPU cores:

| Task | Core | Function |
|------|------|----------|
| LuxSensorTask | 1 | Reads VEML7700 every 10s, updates oneM2M |
| AudioSensorTask | 1 | Samples INMP441, calculates RMS, reports changes |
| OccupancySensorTask | 1 | Polls mmWave GPIO, reports occupancy state |
| NeoPixelUpdate | 0 | Updates LED based on oneM2M state (100ms) |
| NotificationServer | 1 | Handles HTTP callbacks from CSE (10ms) |

### Sensor Operation

**Lux Sensor (VEML7700)**:
- Reads ambient light via I2C
- Reports if change ≥ 1.0 lux
- Updates oneM2M `luxSensor` FlexContainer

**Audio Sensor (INMP441)**:
- Captures audio via I2S
- Calculates RMS loudness
- Reports if change ≥ 5.0 units
- Updates oneM2M `acousticSensor` FlexContainer

**Occupancy Sensor (S3KM1110)**:
- Sends UART configuration on init
- Polls OT2 GPIO for detection state
- Reports on state change
- Updates oneM2M `occupancySensor` FlexContainer

### LED Actuator Operation

**Subscription-based control** (not polling):

1. ESP32 creates HTTP server on port 8888
2. Creates oneM2M subscriptions to `lamp/binarySwitch` and `lamp/color`
3. CSE sends HTTP POST to notification URL when resources change
4. ESP32 parses notification, updates LED state via mutex
5. NeoPixel task applies color changes at 10Hz

**Benefits**:
- Instant response (<100ms vs 5s polling)
- Lower network traffic
- Event-driven architecture
- Scales to multiple actuators

## oneM2M Protocol Details

### Resource Types
- **FlexContainer (28)**: Sensor/actuator data containers
- **Subscription (23)**: Notification registration

### Request Headers
```
X-M2M-Origin: CMoodMonitor
X-M2M-RI: req_{counter}
X-M2M-RVI: 3
Content-Type: application/json;ty={resourceType}
```

### Notification Format
```json
{
  "m2m:sgn": {
    "vrq": false,
    "nev": {
      "rep": {
        "cod:binSh": {
          "state": true
        }
      }
    }
  }
}
```

## Team VibeTribe

**Project**: Mood Monitor
**Competition**: 2025 International oneM2M Hackathon
**System**: Multi-sensor environmental monitoring with intelligent LED feedback

## Dependencies

Managed via PlatformIO (`platformio.ini`):
- Adafruit VEML7700 Library ^2.1.6
- ArduinoJson ^6.21.3
- Adafruit NeoPixel ^1.12.0

## License

MIT
