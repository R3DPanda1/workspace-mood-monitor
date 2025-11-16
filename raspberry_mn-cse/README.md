# VibeTribe Mood Monitor - MN-CSE (Raspberry Pi)

**2025 International oneM2M Hackathon**

Raspberry Pi running a Middle Node Common Services Entity (MN-CSE) that bridges local sensors to the cloud Infrastructure Node (IN-CSE). This node collects data from Bluetooth Low Energy (BLE) SwitchBot sensors and ESP32 devices, then forwards it to the cloud for processing.

## Overview

The MN-CSE acts as the local oneM2M gateway in the home/office environment:
- Runs ACME oneM2M CSE in MN-CSE mode
- Registers with the cloud IN-CSE over WireGuard VPN
- Hosts the `moodMonitorAE` Application Entity
- Collects sensor data from:
  - SwitchBot Meter Plus (CO₂, Temperature, Humidity via BLE)
  - ESP32 Sensor Node (Light, Audio, Occupancy via WiFi)

## Architecture

![System Architecture](../images/full_architecture.png)

## Components

### 1. ACME oneM2M CSE (MN-CSE)
- **Container**: `room-mn-cse`
- **Port**: 8081
- **Image**: `r3dpanda1/acme-onem2m-cse`
- **Purpose**: Local oneM2M server that hosts application resources
- **Configuration**: Connects to cloud IN-CSE at `10.100.0.1:8080`

### 2. SwitchBot Sensor Service
- **Container**: `switchbot-sensor`
- **Language**: Python 3
- **BLE Library**: Bleak (asyncio-based BLE scanner)
- **Purpose**: Scans for SwitchBot Meter Plus, decodes telemetry, posts to MN-CSE
- **Sensors Supported**:
  - Temperature (°C)
  - Humidity (%)
  - CO₂ (ppm → converted to mg/m³)
  - Battery level (%)

## Hardware Requirements

- **Raspberry Pi** (tested on Pi 3B+, 4, or newer)
- **Bluetooth Adapter**: Built-in or USB (hci0)
- **SwitchBot Meter Plus (CO₂ sensor)**: BLE device with manufacturer ID `0x0969`
- **Network**: WireGuard VPN connection to cloud

## OneM2M Resource Structure

The MN-CSE creates and maintains the following resource hierarchy:

```
room-mn-cse (CSE)
├── acpMoodMonitor (ACP)
└── moodMonitorAE (AE)
    └── Room01 (Container)
        └── deviceAirQualityMonitor (FlexContainer: cod:dAQMr)
            ├── airQualitySensor (ModuleClass: cod:aiQSr)
            │   ├── tempe: float (temperature in °C)
            │   ├── humiy: float (humidity %)
            │   └── co2: float (CO₂ in mg/m³)
            └── battery (ModuleClass: cod:bat)
                └── lvl: int (battery level %)
```

### Resource Announcement

The `airQualitySensor` is announced to the cloud IN-CSE, making temperature, humidity, and CO₂ data accessible at the cloud level for analytics.

## Configuration

### Environment Variables

Edit [.env](.env) before starting:

```bash
# MN-CSE Configuration
MNCSE_HOST=10.100.0.2          # MN-CSE IP on WireGuard VPN
MNCSE_PORT=8081                # MN-CSE listening port
MNCSE_ID=id-room-mn-cse        # CSE identifier
MNCSE_NAME=room-mn-cse         # CSE resource name

# IN-CSE Configuration (Cloud)
INCSE_HOST=10.100.0.1          # Cloud IN-CSE IP on WireGuard
INCSE_PORT=8080                # Cloud IN-CSE port
INCSE_ID=id-cloud-in-cse       # Cloud CSE identifier
INCSE_NAME=cloud-in-cse        # Cloud CSE name

# SwitchBot Configuration
SWITCHBOT_MAC=B0:E9:FE:DE:49:2B  # SwitchBot device MAC address
BLE_ADAPTER=hci0                 # Bluetooth adapter name
ORIGINATOR=CMoodMonitor          # oneM2M originator ID
POLL_INTERVAL=60                 # Sensor polling interval (seconds)
ROOM_CONTAINER=Room01            # Room container name
```

## Deployment

### Prerequisites

1. **WireGuard VPN Setup**
   - Follow the [WireGuard tutorial](wireguard_tutorial.md)
   - Ensure VPN is connected: `sudo wg show`

2. **Docker and Docker Compose**
   ```bash
   sudo apt update
   sudo apt install docker.io docker-compose -y
   sudo usermod -aG docker $USER
   ```

3. **Find SwitchBot MAC Address**
   ```bash
   sudo hcitool lescan
   # Look for "WoIOSensorTH" or similar
   ```

### Start the System

```bash
# Clone the repository
cd raspberry_mn-cse

# Configure environment
nano .env
# Update SWITCHBOT_MAC and VPN IPs

# Start services
docker-compose up -d

# Check logs
docker-compose logs -f
```

### Verify Operation

```bash
# Check CSE is running
curl http://localhost:8081/room-mn-cse

# View sensor data
curl -H "X-M2M-Origin: CAdmin" \
     -H "X-M2M-RI: test123" \
     -H "X-M2M-RVI: 3" \
     http://localhost:8081/room-mn-cse/moodMonitorAE/Room01/deviceAirQualityMonitor/airQualitySensor

# Watch sensor logs
docker logs -f switchbot-sensor
```

Expected output:
```
[2025-11-16 10:30:15] === SwitchBot CO2 -> OneM2M MoodMonitor ===
[2025-11-16 10:30:15] Sensor MAC: B0:E9:FE:DE:49:2B
[2025-11-16 10:30:15] CSE: 10.100.0.2:8081
[2025-11-16 10:30:15] Poll interval: 60s
[2025-11-16 10:30:17] CSE is ready
[2025-11-16 10:30:17] Creating AE: moodMonitorAE
[2025-11-16 10:30:18] Hierarchy setup complete
[2025-11-16 10:30:35] Sensor updated: Temp=23.5C, Humidity=45%, CO2=850ppm (1531.2mg/m^3)
```

## SwitchBot Sensor Details

### BLE Manufacturer Data Format

The SwitchBot Meter Plus broadcasts data in BLE advertisements:

| Bytes | Field | Description |
|-------|-------|-------------|
| 8-9 | Temperature | Encoded as: `integer + (fraction * 0.1)`, sign in bit 7 |
| 10 | Humidity | Direct percentage value (0-100) |
| 13-14 | CO₂ | Big-endian 16-bit value in ppm |

### CO₂ Conversion

The sensor converts CO₂ from ppm to mg/m³ using:
```
mg/m³ = (ppm × molecular_weight) / molar_volume
```
Where:
- Molecular weight of CO₂: 44.01 g/mol
- Molar volume: 24.45 L/mol (adjusted for temperature)

## WireGuard VPN Connection

The Raspberry Pi connects to the cloud IN-CSE via WireGuard VPN:

1. Install WireGuard: `sudo apt install wireguard`
2. Copy configuration: `sudo cp configs/wg0.conf /etc/wireguard/`
3. Start VPN: `sudo wg-quick up wg0`
4. Enable on boot: `sudo systemctl enable wg-quick@wg0`

See [wireguard_tutorial.md](wireguard_tutorial.md) for detailed setup.

## Troubleshooting

### BLE Connection Issues
```bash
# Check Bluetooth status
sudo systemctl status bluetooth

# Scan for devices
sudo hcitool lescan

# Reset Bluetooth adapter
sudo hciconfig hci0 down
sudo hciconfig hci0 up
```

### CSE Connection Issues
```bash
# Test VPN connectivity
ping 10.100.0.1

# Check CSE endpoint
curl http://10.100.0.1:8080/cloud-in-cse

# Restart services
docker-compose restart
```

### Container Logs
```bash
# View all logs
docker-compose logs

# Follow specific service
docker-compose logs -f switchbot-sensor
docker-compose logs -f acme-onem2m-cse
```

## File Structure

```
raspberry_mn-cse/
├── docker-compose.yml          # Service orchestration
├── .env                        # Configuration (not in git)
├── wireguard_tutorial.md       # VPN setup guide
├── cse/                        # ACME CSE data volume
└── sensor/                     # SwitchBot sensor service
    ├── Dockerfile.sensor       # Python service container
    └── switchbot-sensor.py     # BLE scanner and oneM2M client
```

## Integration with Cloud

The MN-CSE announces sensor data to the cloud IN-CSE, where:
1. **Ingest Service** normalizes the data and stores it in PostgreSQL
2. **Mood Service** computes workspace mood scores
3. **Grafana** visualizes trends and analytics
4. Results are sent back to control LED indicators on ESP32 devices

## Team VibeTribe

**Project**: Workspace Mood Monitor
**Competition**: 2025 International oneM2M Hackathon
**Hackster.io**: https://www.hackster.io/vibetribe/workspace-mood-monitor-c71c26

## License

MIT
