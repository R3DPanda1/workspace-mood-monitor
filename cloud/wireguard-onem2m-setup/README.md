# WireGuard VPN Setup

Hub-and-spoke VPN connecting edge nodes (MN-CSEs) to cloud IN-CSE.

```
Edge Nodes (10.100.0.2+) → Cloud Hub (10.100.0.1)
```

## Cloud Hub Setup

**1. Install**
```bash
sudo apt update && sudo apt install wireguard
```

**2. Generate Keys**
```bash
cd /etc/wireguard
wg genkey | sudo tee private.key
sudo chmod 600 private.key
sudo cat private.key | wg pubkey | sudo tee public.key
```

**3. Create Config** `/etc/wireguard/wg0.conf`
```ini
[Interface]
Address = 10.100.0.1/24
ListenPort = 51820
PrivateKey = <CLOUD_PRIVATE_KEY>
PostUp = iptables -A FORWARD -i %i -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i %i -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

[Peer]
PublicKey = <EDGE_NODE_PUBLIC_KEY>
AllowedIPs = 10.100.0.2/32, 192.168.1.0/24
```
Replace `eth0` with your server's internet interface (`ip a`).

**4. Firewall & Start**
```bash
sudo ufw allow 51820/udp
sudo systemctl enable --now wg-quick@wg0
sudo wg show
```

## Edge Node Setup

See [../raspberry_mn-cse/wireguard_tutorial.md](../raspberry_mn-cse/wireguard_tutorial.md)

## oneM2M Configuration

**Cloud IN-CSE:** Set `INCSE_HOST=10.100.0.1` in docker-compose.yml
**Edge MN-CSE:** Set `INCSE_HOST=10.100.0.1` in .env

## Troubleshooting

**No handshake:** Check firewall (UDP 51820), verify public keys
**Can't ping:** Check `sysctl net.ipv4.ip_forward`, verify NAT rules
**CSE unreachable:** Test `curl http://10.100.0.1:8080`, check CSE binds to VPN IP
