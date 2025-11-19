# Workspace Mood Monitor

**2025 International oneM2M Hackathon - Team VibeTribe**

IoT system that monitors workspace environmental conditions (CO₂, light, noise, temperature, humidity, occupancy) and computes a "mood score" indicating optimal focus conditions. Visual feedback via RGB LEDs, analytics via Grafana dashboards.

[![Hackster.io](https://img.shields.io/badge/Hackster.io-Project-blue)](https://www.hackster.io/vibetribe/workspace-mood-monitor-c71c26)

![Hardware Setup](images/full_hardware.jpg)

## System Overview

**Sensors** → **Local MN-CSE** (Raspberry Pi) → **WireGuard VPN** → **Cloud IN-CSE** → **Analytics** → **LED Feedback**

![Architecture](images/full_architecture.png)

### Components

| Layer | Component | Purpose |
|-------|-----------|---------|
| **Sensors** | ESP32-S3 + VEML7700, INMP441, S3KM1110 | Light, audio, occupancy |
| | SwitchBot Meter Plus (BLE) | CO₂, temperature, humidity |
| **Local** | Raspberry Pi MN-CSE (ACME) | oneM2M local server |
| **Cloud** | IN-CSE (ACME) | oneM2M cloud server |
| | Ingest Service (Flask) | Normalize telemetry |
| | Mood ML Service (FastAPI) | ML-powered mood scoring & LED control |
| | PostgreSQL | Store telemetry & mood data |
| | Grafana | Dashboards |
| **Network** | WireGuard VPN | Secure site-to-cloud tunnel |

**Result**: Score + Label (focus/neutral/tired) + LED color (green/yellow/red)

## Quick Start

### 1. Cloud (VPS with public IP)
```bash
cd cloud
cp .env.example .env  # Configure IPs, passwords
docker-compose up -d
```
Access Grafana: `http://your-vps:3000` (admin/your_password)

### 2. WireGuard VPN
Setup on cloud and Raspberry Pi - see [raspberry_mn-cse/wireguard_tutorial.md](raspberry_mn-cse/wireguard_tutorial.md)

### 3. Raspberry Pi
```bash
cd raspberry_mn-cse
cp .env.example .env  # Configure CSE IPs, SwitchBot MAC
docker-compose up -d
```

### 4. ESP32 Sensor Node
```bash
cd esp32_sensornode
# Edit include/config.h with WiFi, CSE IP
pio run -t upload
```

## Repository Structure

```
├── esp32_sensornode/          # ESP32-S3 firmware (PlatformIO)
├── raspberry_mn-cse/          # Raspberry Pi MN-CSE + BLE service
├── cloud/                     # Cloud IN-CSE + analytics
│   ├── ingest/                # Flask normalization service
│   ├── mood-service-ml/       # ML-powered mood computation (FastAPI)
│   ├── postgres/              # Database schema + migrations
│   ├── grafana/               # Dashboard provisioning
│   └── wireguard-onem2m-setup/ # VPN config examples
└── images/                    # Documentation images
```

## Hardware

- ESP32-S3-DevKitC-1
- Raspberry Pi 3B+/4
- VEML7700 (light)
- INMP441 (audio)
- S3KM1110 (occupancy)
- SwitchBot Meter Plus (CO₂/temp/humidity)
- WS2812 RGB LED

## Documentation

- [ESP32 Sensor Node](esp32_sensornode/README.md)
- [Raspberry Pi MN-CSE](raspberry_mn-cse/README.md)
- [Cloud Platform](cloud/README.md)
- [WireGuard VPN Setup](raspberry_mn-cse/wireguard_tutorial.md)

## Team

**Alper Ramadan, Benjamin Karic, Tahir Toy**

[Hackster.io Project](https://www.hackster.io/vibetribe/workspace-mood-monitor-c71c26)

## License

MIT
