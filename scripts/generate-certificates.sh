#!/bin/bash
#
# Certificate Generation Script for ACME oneM2M CSE TLS
#
# This script generates:
# 1. A Certificate Authority (CA) for signing certificates
# 2. Server certificates for both IN-CSE (cloud) and MN-CSE (raspberry pi)
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Base directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Certificate directories
CERTS_DIR="$PROJECT_ROOT/certs"
CLOUD_CERT_DIR="$PROJECT_ROOT/cloud/cse/cert"
RASPBERRY_CERT_DIR="$PROJECT_ROOT/raspberry_mn-cse/cse/cert"

echo -e "${GREEN}=== ACME CSE Certificate Generation ===${NC}"

# Create certificate directories
mkdir -p "$CERTS_DIR"
mkdir -p "$CLOUD_CERT_DIR"
mkdir -p "$RASPBERRY_CERT_DIR"

cd "$CERTS_DIR"

# Step 1: Generate CA private key
echo -e "\n${YELLOW}Step 1: Generating Certificate Authority (CA) private key...${NC}"
if [ ! -f ca.key ]; then
    openssl genrsa -out ca.key 4096
    echo -e "${GREEN}✓ CA private key generated: ca.key${NC}"
else
    echo -e "${YELLOW}! CA private key already exists: ca.key${NC}"
fi

# Step 2: Generate self-signed CA certificate
echo -e "\n${YELLOW}Step 2: Generating self-signed CA certificate...${NC}"
if [ ! -f ca.crt ]; then
    openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
        -subj "/C=US/ST=State/L=City/O=VibeTribe/OU=IoT/CN=VibeTribe-CA"
    echo -e "${GREEN}✓ CA certificate generated: ca.crt${NC}"
else
    echo -e "${YELLOW}! CA certificate already exists: ca.crt${NC}"
fi

# Step 3: Generate Cloud IN-CSE certificate
echo -e "\n${YELLOW}Step 3: Generating Cloud IN-CSE certificate...${NC}"

# Create OpenSSL config for cloud CSE
cat > cloud-cse.conf <<EOF
[req]
default_bits = 4096
prompt = no
default_md = sha256
distinguished_name = dn
req_extensions = req_ext

[dn]
C = US
ST = State
L = City
O = VibeTribe
OU = Cloud
CN = cloud-in-cse

[req_ext]
subjectAltName = @alt_names

[alt_names]
DNS.1 = cloud-in-cse
DNS.2 = acme
DNS.3 = localhost
IP.1 = 127.0.0.1
EOF

# Generate cloud CSE private key
if [ ! -f cloud-cse.key ]; then
    openssl genrsa -out cloud-cse.key 4096
    echo -e "${GREEN}✓ Cloud CSE private key generated: cloud-cse.key${NC}"
else
    echo -e "${YELLOW}! Cloud CSE private key already exists: cloud-cse.key${NC}"
fi

# Generate cloud CSE certificate signing request
openssl req -new -key cloud-cse.key -out cloud-cse.csr -config cloud-cse.conf

# Create extensions file for signing
cat > cloud-cse.ext <<EOF
subjectAltName = @alt_names

[alt_names]
DNS.1 = cloud-in-cse
DNS.2 = acme
DNS.3 = localhost
IP.1 = 127.0.0.1
EOF

# Sign the cloud CSE certificate
if [ ! -f cloud-cse.crt ]; then
    openssl x509 -req -in cloud-cse.csr -CA ca.crt -CAkey ca.key \
        -CAcreateserial -out cloud-cse.crt -days 3650 \
        -extfile cloud-cse.ext
    echo -e "${GREEN}✓ Cloud CSE certificate generated: cloud-cse.crt${NC}"
else
    echo -e "${YELLOW}! Cloud CSE certificate already exists: cloud-cse.crt${NC}"
fi

# Step 4: Generate Raspberry Pi MN-CSE certificate
echo -e "\n${YELLOW}Step 4: Generating Raspberry Pi MN-CSE certificate...${NC}"

# Create OpenSSL config for raspberry CSE
cat > raspberry-cse.conf <<EOF
[req]
default_bits = 4096
prompt = no
default_md = sha256
distinguished_name = dn
req_extensions = req_ext

[dn]
C = US
ST = State
L = City
O = VibeTribe
OU = Edge
CN = room-mn-cse

[req_ext]
subjectAltName = @alt_names

[alt_names]
DNS.1 = room-mn-cse
DNS.2 = acme-onem2m-cse
DNS.3 = localhost
IP.1 = 127.0.0.1
IP.2 = 10.6.0.2
EOF

# Generate raspberry CSE private key
if [ ! -f raspberry-cse.key ]; then
    openssl genrsa -out raspberry-cse.key 4096
    echo -e "${GREEN}✓ Raspberry CSE private key generated: raspberry-cse.key${NC}"
else
    echo -e "${YELLOW}! Raspberry CSE private key already exists: raspberry-cse.key${NC}"
fi

# Generate raspberry CSE certificate signing request
openssl req -new -key raspberry-cse.key -out raspberry-cse.csr -config raspberry-cse.conf

# Create extensions file for signing
cat > raspberry-cse.ext <<EOF
subjectAltName = @alt_names

[alt_names]
DNS.1 = room-mn-cse
DNS.2 = acme-onem2m-cse
DNS.3 = localhost
IP.1 = 127.0.0.1
IP.2 = 10.6.0.2
EOF

# Sign the raspberry CSE certificate
if [ ! -f raspberry-cse.crt ]; then
    openssl x509 -req -in raspberry-cse.csr -CA ca.crt -CAkey ca.key \
        -CAcreateserial -out raspberry-cse.crt -days 3650 \
        -extfile raspberry-cse.ext
    echo -e "${GREEN}✓ Raspberry CSE certificate generated: raspberry-cse.crt${NC}"
else
    echo -e "${YELLOW}! Raspberry CSE certificate already exists: raspberry-cse.crt${NC}"
fi

# Step 5: Copy certificates to appropriate locations
echo -e "\n${YELLOW}Step 5: Copying certificates to service directories...${NC}"

# Copy to cloud directory
cp ca.crt ca.key cloud-cse.crt cloud-cse.key "$CLOUD_CERT_DIR/"
echo -e "${GREEN}✓ Cloud certificates copied to: $CLOUD_CERT_DIR/${NC}"

# Copy to raspberry directory
cp ca.crt ca.key raspberry-cse.crt raspberry-cse.key "$RASPBERRY_CERT_DIR/"
echo -e "${GREEN}✓ Raspberry certificates copied to: $RASPBERRY_CERT_DIR/${NC}"

# Step 6: Verify certificates
echo -e "\n${YELLOW}Step 6: Verifying certificates...${NC}"

echo -e "\n${GREEN}Cloud IN-CSE Certificate:${NC}"
openssl x509 -in cloud-cse.crt -text -noout | grep -A 3 "Subject Alternative Name"

echo -e "\n${GREEN}Raspberry MN-CSE Certificate:${NC}"
openssl x509 -in raspberry-cse.crt -text -noout | grep -A 3 "Subject Alternative Name"

# Clean up temporary files
rm -f *.csr *.conf *.ext *.srl

echo -e "\n${GREEN}=== Certificate Generation Complete ===${NC}"
echo -e "\n${YELLOW}Generated certificates:${NC}"
echo -e "  CA Certificate: ${GREEN}ca.crt${NC}"
echo -e "  CA Private Key: ${GREEN}ca.key${NC}"
echo -e "  Cloud CSE Certificate: ${GREEN}cloud-cse.crt${NC}"
echo -e "  Cloud CSE Private Key: ${GREEN}cloud-cse.key${NC}"
echo -e "  Raspberry CSE Certificate: ${GREEN}raspberry-cse.crt${NC}"
echo -e "  Raspberry CSE Private Key: ${GREEN}raspberry-cse.key${NC}"
echo -e "\n${YELLOW}Next steps:${NC}"
echo -e "  1. Review the updated acme.ini files in cloud/cse/ and raspberry_mn-cse/cse/"
echo -e "  2. Update docker-compose.yml files if needed"
echo -e "  3. Restart your CSE services: ${GREEN}docker-compose down && docker-compose up -d${NC}"
echo -e "\n${RED}IMPORTANT: Keep ca.key and *.key files secure and never commit them to version control!${NC}"
