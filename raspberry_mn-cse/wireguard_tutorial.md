# WireGuard VPN Setup

## Installation
sudo apt update && sudo apt install wireguard resolvconf

## Configuration
# Place config in /etc/wireguard/wg0.conf
sudo cp wg0.conf /etc/wireguard/wg0.conf
sudo chmod 600 /etc/wireguard/wg0.conf

## Service Management
sudo systemctl enable --now wg-quick@wg0  # Enable & start
sudo systemctl restart wg-quick@wg0        # Restart
sudo systemctl status wg-quick@wg0         # Check status

# Manual control
sudo wg-quick down wg0
sudo wg-quick up wg0

## Verification
sudo wg show                # Show current status
ping -c 4 10.100.0.1        # Test VPN gateway

## Config file
sudo cat /etc/wireguard/wg0.conf

	# WireGuard client configuration
	[Interface]
	# VPN address assigned to this Pi
	Address = 10.100.0.2/32
	# Pi's private key (keep secret!)
	PrivateKey = YOUR_PRIVATE_KEY
	# Optional DNS resolver
	DNS = 1.1.1.1
	# Enable forwarding + permit wg0 <-> wlan0 + NAT LAN out wg0
	PostUp   = sysctl -w net.ipv4.ip_forward=1
	PostUp   = iptables -A FORWARD -i wg0 -o wlan0 -j ACCEPT
	PostUp   = iptables -A FORWARD -i wlan0 -o wg0 -j ACCEPT
	PostUp   = iptables -t nat -A POSTROUTING -s 192.168.123.0/24 -o wg0 -j MASQUERADE
	PostDown = sysctl -w net.ipv4.ip_forward=0
	PostDown = iptables -D FORWARD -i wg0 -o wlan0 -j ACCEPT
	PostDown = iptables -D FORWARD -i wlan0 -o wg0 -j ACCEPT
	PostDown = iptables -t nat -D POSTROUTING -s 192.168.123.0/24 -o wg0 -j MASQUERADE

	[Peer]
	# Cloud hub server
	PublicKey = SERVER_PUBLIC_KEY
	# Cloud server endpoint (replace if IP/port change)
	Endpoint = SERVER_IP:51820
	# Routes to send over the tunnel
	AllowedIPs = 10.100.0.0/24
	# Keep NAT bindings alive if idle
	PersistentKeepalive = 25