# WireGuard VPN Setup for oneM2M Deployment

Simple hub-and-spoke VPN connecting edge nodes (MN-CSEs on Raspberry Pis) to cloud IN-CSE.

## Network Topology

```
Edge Nodes (MN-CSE)              Cloud Hub (IN-CSE)
10.100.0.2, 10.100.0.3, ...  →   10.100.0.1
Behind home NAT                   Public IP server
```

## Cloud Hub Setup (IN-CSE Server)

### 1. Install WireGuard
```bash
sudo apt update && sudo apt install wireguard
```

### 2. Generate Keys
```bash
cd /etc/wireguard
wg genkey | sudo tee private.key
sudo chmod 600 private.key
sudo cat private.key | wg pubkey | sudo tee public.key

echo "Cloud Hub Public Key (share with edge nodes):"
sudo cat public.key
```

### 3. Create Config
```bash
sudo nano /etc/wireguard/wg0.conf
```

```ini
[Interface]
Address = 10.100.0.1/24
ListenPort = 51820
PrivateKey = <CLOUD_PRIVATE_KEY>
PostUp = iptables -A FORWARD -i %i -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i %i -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

# Edge Node 1
[Peer]
PublicKey = <EDGE_NODE_1_PUBLIC_KEY>
AllowedIPs = 10.100.0.2/32, 192.168.1.0/24

# Edge Node 2 (add more as needed)
[Peer]
PublicKey = <EDGE_NODE_2_PUBLIC_KEY>
AllowedIPs = 10.100.0.3/32, 192.168.2.0/24
```

Replace `eth0` with your server's internet interface (check with `ip a`).

### 4. Configure Firewall
```bash
sudo ufw allow 51820/udp
sudo ufw reload
```

### 5. Start WireGuard
```bash
sudo systemctl enable --now wg-quick@wg0
sudo wg show  # Verify
```

## Edge Node Setup (Raspberry Pi MN-CSE)

See [../raspberry_mn-cse/wireguard_tutorial.md](../raspberry_mn-cse/wireguard_tutorial.md) for detailed edge node setup.

**Quick summary:**
1. Install: `sudo apt install wireguard resolvconf`
2. Generate keys: `wg genkey | tee private.key | wg pubkey > public.key`
3. Share public key with cloud admin
4. Create `/etc/wireguard/wg0.conf` with cloud hub endpoint
5. Start: `sudo systemctl enable --now wg-quick@wg0`

## Verification

### On Cloud Hub
```bash
sudo wg show
# Should show connected peers with recent handshake
```

### On Edge Node
```bash
sudo wg show
ping -c 4 10.100.0.1  # Ping cloud hub
```

## Configure oneM2M to Use VPN

### Cloud IN-CSE
Listen on VPN interface:
```bash
# In docker-compose.yml or CSE config
INCSE_HOST=10.100.0.1
```

### Edge MN-CSE
Point to cloud hub via VPN:
```bash
# In .env or MN-CSE config
INCSE_HOST=10.100.0.1
INCSE_PORT=8080
```

## Troubleshooting

**No handshake:**
- Check cloud firewall allows UDP 51820
- Verify public keys are correct on both sides
- Ensure cloud hub is running: `sudo systemctl status wg-quick@wg0`

**Handshake works but can't ping:**
- Check IP forwarding: `sysctl net.ipv4.ip_forward` (should be 1)
- Verify NAT rules: `sudo iptables -t nat -L`

**VPN works but CSE can't connect:**
- Test CSE endpoint: `curl http://10.100.0.1:8080`
- Check CSE is listening on VPN IP: `netstat -tulpn | grep 8080`

## Files

- `configs/cloud-hub-example.conf` - Cloud hub WireGuard config template
- `configs/edge-node-example.conf` - Edge node WireGuard config template
- `scripts/` - Helper scripts for key generation and setup
