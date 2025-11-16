# Cloud Infrastructure (IN-CSE)

**2025 International oneM2M Hackathon**

Cloud platform for the Workspace Mood Monitor system. Runs oneM2M IN-CSE, ingests sensor telemetry, computes mood scores, controls LED feedback, and provides Grafana dashboards.

![Grafana Dashboard](../images/Grafana.png)

## Services

| Service | Purpose | Port |
|---------|---------|------|
| **acme** | oneM2M IN-CSE server | 8080 |
| **ingest** | Normalize telemetry, store in DB | 8088 |
| **ingest-worker** | Background queue processor | - |
| **mood** | Compute mood scores, control LEDs | 8087 |
| **postgres** | Database (telemetry + mood) | 5432 (internal) |
| **grafana** | Dashboards | 3000 |

## Setup

### Prerequisites
- Cloud VPS with public IP
- Docker + Docker Compose
- Ports: 8080 (CSE), 3000 (Grafana), 51820 (WireGuard)

### 1. Environment Configuration

Configure `docker-compose.yml` environment variables:
- `POSTGRES_PASSWORD` - Database password
- `GRAFANA_ADMIN_PASSWORD` - Grafana login
- `INCSE_HOST=10.100.0.1` - Cloud VPN IP
- `MNCSE_HOST=10.100.0.2` - Raspberry Pi VPN IP

### 2. Start Services

```bash
docker-compose up -d
docker ps  # Verify all containers running
```

### 3. Apply Database Migrations

```bash
for f in postgres/migrations/*.sql; do
  docker exec -i onem2m_postgres psql -U onem2m -d onem2m < "$f"
done
```

### 4. Access Grafana

Open `http://your-server-ip:3000`
- Username: `admin`
- Password: (set in `docker-compose.yml`)

## Verification

```bash
# Check CSE
curl http://localhost:8080/~/id-cloud-in-cse

# View telemetry
docker exec -i onem2m_postgres psql -U onem2m -d onem2m \
  -c "SELECT * FROM fact_telemetry ORDER BY ts_cse DESC LIMIT 5;"

# View mood scores
docker exec -i onem2m_postgres psql -U onem2m -d onem2m \
  -c "SELECT * FROM fact_mood ORDER BY inserted_at DESC LIMIT 5;"
```

## WireGuard VPN

See [wireguard-onem2m-setup/README.md](wireguard-onem2m-setup/README.md) for VPN setup.

**Network**: `10.100.0.0/24`
- Cloud hub: `10.100.0.1`
- Edge nodes: `10.100.0.2-4`

## Data Flow

```
MN-CSE → WireGuard VPN → IN-CSE (8080)
                            ↓
                      Ingest (8088)
                            ↓
                      PostgreSQL
                            ↓
                    Mood Service (8087)
                            ↓
                ┌───────────┴────────────┐
                ↓                        ↓
           PostgreSQL               LED Control
                ↓                    (via IN-CSE)
            Grafana (3000)
```

## Supported Metrics

| Metric | Synonyms | Unit |
|--------|----------|------|
| temperature | temp, tempe | °C |
| humidity | rh, humiy | % |
| co2 | co2ppm | ppm |
| lux | lux | lux |
| noise | louds | dB |
| occupancy | occ | count |

## File Structure

```
cloud/
├── docker-compose.yml
├── cse/                       # ACME CSE config
├── ingest/                    # Flask normalization service
├── mood-service/              # FastAPI mood computation
├── postgres/                  # Database init + migrations
├── grafana/                   # Dashboard provisioning
└── wireguard-onem2m-setup/    # VPN tutorial + examples
```

## Troubleshooting

**Services won't start:**
```bash
docker-compose logs
docker-compose restart
```

**Database connection issues:**
```bash
docker exec -i onem2m_postgres psql -U onem2m -d onem2m -c "SELECT version();"
```

**CSE not accessible:**
```bash
docker ps | grep cloud-in-cse
docker logs cloud-in-cse --tail 50
```

## Security

- Change default passwords in `docker-compose.yml`
- Use firewall to restrict port access
- Only expose WireGuard (51820/udp) to public internet
- Access CSE/Grafana via VPN or reverse proxy with HTTPS

## Team VibeTribe

**Alper Ramadan, Benjamin Karic, Tahir Toy**

[Hackster.io Project](https://www.hackster.io/vibetribe/workspace-mood-monitor-c71c26)

## License

MIT
