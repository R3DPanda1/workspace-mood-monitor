# VibeTribe Mood Monitor - Cloud Infrastructure (IN-CSE)

**2025 International oneM2M Hackathon**

Cloud infrastructure running the oneM2M IN-CSE (Infrastructure Node) that receives telemetry from distributed MN-CSEs, computes workspace mood analytics, and provides visualization dashboards. The system ingests sensor data via oneM2M, normalizes it, computes mood scores, and controls LED feedback indicators.

![Grafana Dashboard](../images/Grafana.png)

## Overview

The Cloud IN-CSE acts as the central hub for the workspace mood monitoring system:
- Runs ACME oneM2M CSE in IN-CSE mode
- Receives telemetry from remote MN-CSEs over WireGuard VPN
- Ingests and normalizes sensor data (CO₂, temperature, humidity, light, noise, occupancy)
- Computes heuristic mood scores and controls LED indicators
- Stores analytics in PostgreSQL database
- Visualizes trends in Grafana dashboards

## Architecture

![System Architecture](../images/full_architecture.png)

## Components

### 1. ACME oneM2M CSE (IN-CSE)
- **Container**: `cloud-in-cse`
- **Port**: 8080
- **Image**: `r3dpanda1/acme-onem2m-cse`
- **Purpose**: Central oneM2M server that receives telemetry from MN-CSEs
- **Configuration**: Accepts registrations from remote MN-CSEs via WireGuard VPN

### 2. Ingest Service
- **Container**: `ingest`
- **Port**: 8088
- **Language**: Python (Flask)
- **Purpose**: Receives oneM2M notifications, normalizes telemetry, stores in database
- **Features**:
  - Handles multiple payload formats (compact, flat, nested)
  - Normalizes metric synonyms (temp/tempe/temperature, rh/humiy/humidity, etc.)
  - Stores raw payloads and normalized telemetry
  - Forwards to mood service

### 3. Ingest Worker
- **Container**: `ingest-worker`
- **Language**: Python
- **Purpose**: Background processing of queued telemetry data
- **Features**: Retry logic, batch processing, fault tolerance

### 4. Mood Service
- **Container**: `mood`
- **Port**: 8088 (internal), 8087 (host)
- **Language**: Python (FastAPI)
- **Purpose**: Computes mood scores and controls LED indicators
- **Algorithm**: Weighted heuristic based on:
  - CO₂ (25%) - optimal: 400-800 ppm
  - Noise (20%) - optimal: 30-50 dB
  - Light (20%) - optimal: 300-500 lux
  - Temperature (15%) - optimal: 20-24°C
  - Humidity (10%) - optimal: 30-60%
  - Occupancy (10%) - presence detection
- **Output**: Score (0-100), label (focus/neutral/tired), LED color (red→green)

### 5. PostgreSQL Database
- **Container**: `onem2m_postgres`
- **Port**: 5432 (internal)
- **Version**: PostgreSQL 15
- **Schema**: Star schema with dimensions and facts
  - `dim_room`, `dim_device`, `dim_metric` (dimensions)
  - `fact_telemetry` (normalized sensor readings)
  - `fact_mood` (computed mood scores and LED colors)
  - `raw_onem2m_ci` (raw oneM2M content instances)

### 6. Grafana
- **Container**: `grafana`
- **Port**: 3000
- **Purpose**: Real-time dashboards for telemetry and mood analytics
- **Datasource**: PostgreSQL

## OneM2M Data Flow

```
Sensors (MN-CSE)
    ↓ oneM2M CIN POST via WireGuard VPN
ACME IN-CSE (port 8080)
    ↓ Subscription notification
Ingest Service (port 8088)
    ↓ Normalize & persist
PostgreSQL (fact_telemetry)
    ↓ Forward normalized data
Mood Service (port 8088)
    ↓ Compute mood score
PostgreSQL (fact_mood)
    ↓ POST mood CIN
ACME IN-CSE (mood container)
    ↓ PUT LED color
Lamp (MN-CSE)
    ↓ Read from database
Grafana Dashboards (port 3000)
```

## Supported Metrics

The system normalizes the following canonical metrics with their synonyms:

| Metric | Synonyms | Unit |
|--------|----------|------|
| temperature | temp, tempe | °C |
| humidity | rh, humiy | % |
| co2 | co2ppm | ppm |
| lux | lux | lux |
| noise | louds | dB |
| occupancy | occ | count |

## Configuration

### Prerequisites

1. **Cloud Server Requirements**
   - Ubuntu 20.04+ or similar Linux distribution
   - Docker and Docker Compose installed
   - Public IP address
   - Ports available: 8080 (CSE), 8087-8088 (services), 3000 (Grafana), 51820 (WireGuard)

2. **WireGuard VPN Setup**
   - Follow [wireguard-onem2m-setup/README.md](wireguard-onem2m-setup/README.md)
   - VPN subnet: `10.100.0.0/24`
   - Cloud hub: `10.100.0.1`
   - Edge nodes: `10.100.0.2-4`

### Environment Variables

Copy the example environment file and configure:

```bash
cp .env.example .env
nano .env
```

Required variables:

```bash
# Database
POSTGRES_USER=your_db_user
POSTGRES_PASSWORD=your_secure_password
POSTGRES_DB=onem2m

# Cloud IN-CSE
INCSE_HOST=10.100.0.1          # Cloud WireGuard IP
INCSE_PORT=8080
INCSE_ID=id-cloud-in-cse
INCSE_NAME=cloud-in-cse

# Expected MN-CSE
MNCSE_ID=id-room-mn-cse
MNCSE_NAME=room-mn-cse
MNCSE_HOST=10.100.0.2          # Raspberry Pi WireGuard IP
MNCSE_PORT=8081

# Service configuration
CSE_BASE=http://cloud-in-cse:8080/~/id-room-mn-cse/-/moodMonitorAE/Room01/moodAnalysis
CSE_ORIGIN=CAdmin
MOOD_NOTIFY=http://mood:8088/notify
ROOM_IDS=room-101,room-102

# Grafana
GRAFANA_ADMIN_PASSWORD=your_grafana_password

# LED control
CSE_PUT_BASE=http://10.100.0.1:8080
CSE_PUT_RVI=3
CSE_PUT_CSEID=id-room-mn-cse
CSE_PUT_AE=moodMonitorAE
```

## Deployment

### 1. Setup WireGuard VPN

Follow [wireguard-onem2m-setup/README.md](wireguard-onem2m-setup/README.md) for VPN setup instructions.

### 2. Start the Services

```bash
# Start all services
docker-compose up -d

# Verify services are running
docker ps

# Check logs
docker-compose logs -f
```

### 3. Apply Database Migrations

```bash
# Apply all migrations
docker exec -i onem2m_postgres psql -U onem2m -d onem2m < postgres/migrations/001_seed_dim_metric.sql
docker exec -i onem2m_postgres psql -U onem2m -d onem2m < postgres/migrations/002_create_fact_mood.sql
docker exec -i onem2m_postgres psql -U onem2m -d onem2m < postgres/migrations/003_ingest_queue.sql
docker exec -i onem2m_postgres psql -U onem2m -d onem2m < postgres/migrations/004_allow_duplicate_ci_and_fact.sql
docker exec -i onem2m_postgres psql -U onem2m -d onem2m < postgres/migrations/005_provenance_and_indexes.sql
docker exec -i onem2m_postgres psql -U onem2m -d onem2m < postgres/migrations/006_add_led_to_fact_mood.sql
```

### 4. Verify Operation

```bash
# Check CSE is accessible
curl http://localhost:8080/~/id-cloud-in-cse

# View recent telemetry
docker exec -i onem2m_postgres psql -U onem2m -d onem2m \
  -c "SELECT * FROM fact_telemetry ORDER BY ts_cse DESC LIMIT 10;"

# View mood scores
docker exec -i onem2m_postgres psql -U onem2m -d onem2m \
  -c "SELECT * FROM fact_mood ORDER BY inserted_at DESC LIMIT 10;"
```

### 5. Access Grafana

Open your browser to `http://<your-server-ip>:3000`

- Username: `admin`
- Password: (from `GRAFANA_ADMIN_PASSWORD` in `.env`)

Dashboards are pre-configured to show:
- Real-time sensor values
- Mood score gauge
- Historical trends
- Room status

## Testing

### Manual Test: Post Telemetry

```bash
# Post a test telemetry sample
curl -X POST "http://localhost:8080/~/id-cloud-in-cse/-/cloud-analytics/telemetry/room-101/sample" \
  -H "Content-Type: application/json;ty=4" \
  -H "X-M2M-Origin: CAdmin" \
  -H "X-M2M-RI: $(uuidgen)" \
  -H "X-M2M-RVI: 4" \
  -d '{
    "m2m:cin": {
      "con": {
        "tempe": 22.5,
        "humiy": 50,
        "co2": 650,
        "lux": 400,
        "noise": 45,
        "occ": 1
      },
      "cnf": "application/json:0"
    }
  }'
```

### Read Latest Mood

```bash
# From CSE
curl -s "http://localhost:8080/~/id-cloud-in-cse/-/cloud-analytics/analytics/mood/score/la" | jq

# From mood service API
curl -s "http://localhost:8087/latest-mood" | jq
```

### Integration Test

Post a test telemetry sample and verify it flows through the system:

```bash
# Post test data
curl -X POST "http://localhost:8080/~/id-cloud-in-cse/-/cloud-analytics/telemetry/room-101/sample" \
  -H "Content-Type: application/json;ty=4" \
  -H "X-M2M-Origin: CAdmin" \
  -H "X-M2M-RI: test-$(date +%s)" \
  -H "X-M2M-RVI: 4" \
  -d '{
    "m2m:cin": {
      "con": {"tempe": 22, "humiy": 50, "co2": 650, "lux": 400, "noise": 45, "occ": 1},
      "cnf": "application/json:0"
    }
  }'

# Verify data in database
docker exec -i onem2m_postgres psql -U onem2m -d onem2m \
  -c "SELECT * FROM fact_telemetry ORDER BY ts_cse DESC LIMIT 5;"

# Check mood was computed
docker exec -i onem2m_postgres psql -U onem2m -d onem2m \
  -c "SELECT * FROM fact_mood ORDER BY inserted_at DESC LIMIT 5;"
```

## WireGuard VPN Setup

The system uses WireGuard VPN to securely connect distributed MN-CSEs to the cloud IN-CSE.

### Network Topology

- **VPN Subnet**: `10.100.0.0/24`
- **Cloud Hub**: `10.100.0.1` (this server)
- **Team Members**: `10.100.0.2-4` (Raspberry Pis)

### Setup Instructions

1. **Install WireGuard**
   ```bash
   sudo apt-get update
   sudo apt-get install wireguard -y
   ```

2. **Configure WireGuard**
   - See [wireguard-onem2m-setup/README.md](wireguard-onem2m-setup/README.md) for setup instructions
   - Config examples in [wireguard-onem2m-setup/configs/](wireguard-onem2m-setup/configs/)

3. **Start WireGuard**
   ```bash
   sudo systemctl enable --now wg-quick@wg0
   ```

4. **Verify Connection**
   ```bash
   sudo wg show
   ping 10.100.0.2  # Test connection to team member Pi
   ```

## Troubleshooting

### Service Issues

```bash
# View all container logs
docker-compose logs

# View specific service logs
docker-compose logs -f ingest
docker-compose logs -f mood

# Restart services
docker-compose restart

# Rebuild and restart
docker-compose up -d --build
```

### Database Issues

```bash
# Check database connection
docker exec -i onem2m_postgres psql -U onem2m -d onem2m -c "SELECT version();"

# View table contents
docker exec -i onem2m_postgres psql -U onem2m -d onem2m -c "\dt"

# Check recent errors in logs
docker logs onem2m_postgres --tail 100
```

### CSE Connection Issues

```bash
# Verify CSE is running
docker ps | grep cloud-in-cse

# Check CSE logs
docker logs cloud-in-cse --tail 100

# Test CSE endpoint
curl -v http://localhost:8080/~/id-cloud-in-cse
```

### VPN Issues

```bash
# Check WireGuard status
sudo systemctl status wg-quick@wg0

# View WireGuard interface
sudo wg show

# Check firewall rules
sudo ufw status
sudo iptables -L -n -v

# Test VPN connectivity
ping 10.100.0.2  # Ping team member Pi
```

## File Structure

```
cloud/
├── docker-compose.yml          # Service orchestration
├── .env.example                # Environment template (copy to .env)
├── .gitignore                  # Git ignore rules
│
├── cse/                        # ACME CSE configuration
│   ├── acme.ini                # CSE settings
│   └── init/
│       └── mio_sensors.fcp     # Sensor initialization
│
├── ingest/                     # Ingest service
│   ├── app.py                  # Flask application
│   ├── worker.py               # Background worker
│   ├── Dockerfile
│   └── requirements.txt
│
├── mood-service/               # Mood computation service
│   ├── app.py                  # FastAPI application
│   ├── Dockerfile
│   └── requirements.txt
│
├── postgres/                   # Database configuration
│   ├── init.sql                # Initial schema
│   └── migrations/             # Database migrations
│       ├── 001_seed_dim_metric.sql
│       ├── 002_create_fact_mood.sql
│       ├── 003_ingest_queue.sql
│       ├── 004_allow_duplicate_ci_and_fact.sql
│       ├── 005_provenance_and_indexes.sql
│       └── 006_add_led_to_fact_mood.sql
│
├── grafana/                    # Grafana configuration
│   └── provisioning/
│       └── datasources/
│           └── datasource.yml  # PostgreSQL datasource
│
└── wireguard-onem2m-setup/     # WireGuard VPN setup
    ├── README.md               # Setup tutorial
    └── configs/                # Config examples
        ├── cloud-hub-example.conf
        └── edge-node-example.conf
```

## Security Considerations

### Before Deployment

1. **Change default passwords** in `.env`:
   - `POSTGRES_PASSWORD`
   - `GRAFANA_ADMIN_PASSWORD`

2. **Secure WireGuard keys**:
   - Never commit `wg0.conf` with private keys
   - Use `.gitignore` to exclude sensitive files
   - Keep private keys with 600 permissions

3. **Configure firewall**:
   ```bash
   # Allow WireGuard
   sudo ufw allow 51820/udp

   # Allow Grafana (optional, use reverse proxy)
   sudo ufw allow 3000/tcp

   # Deny direct access to services (use VPN only)
   sudo ufw deny 8080/tcp
   sudo ufw deny 8087/tcp
   sudo ufw deny 8088/tcp
   ```

4. **Restrict CSE access**:
   - Configure Access Control Policies (ACPs) in ACME
   - Use IP allowlisting for `/notify` endpoints
   - Consider using TLS/HTTPS for production

### Production Recommendations

- Use Docker secrets instead of `.env` for sensitive data
- Enable TLS for oneM2M communication
- Set up automated database backups with `pg_dump`
- Configure log rotation
- Monitor system metrics with Prometheus
- Use reverse proxy (nginx) for Grafana with HTTPS

## Integration with Edge Devices

The cloud IN-CSE receives data from:

1. **Raspberry Pi MN-CSE** - See [../raspberry_mn-cse/](../raspberry_mn-cse/)
   - SwitchBot sensors (CO₂, temperature, humidity via BLE)
   - Registers with cloud IN-CSE via WireGuard VPN

2. **ESP32 Sensor Node** - See [../esp32_sensornode/](../esp32_sensornode/)
   - Light, audio, occupancy sensors
   - LED feedback indicator
   - Connects to local MN-CSE

Data flows: ESP32 → MN-CSE → VPN → IN-CSE → Ingest → Mood Service → LED Control

## Team VibeTribe

**Project**: Workspace Mood Monitor
**Competition**: 2025 International oneM2M Hackathon
**Team**: Alper Ramadan, Benjamin Karic, Tahir Toy
**Hackster.io**: https://www.hackster.io/vibetribe/workspace-mood-monitor-c71c26

## License

MIT
