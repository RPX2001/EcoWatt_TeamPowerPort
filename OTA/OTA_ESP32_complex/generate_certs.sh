#!/bin/bash

# Generate SSL Certificates for Secure OTA Server
# This script creates self-signed certificates for local development

set -e

CERT_DIR="certs"
SERVER_KEY="server.key"
SERVER_CRT="server.crt"
CA_KEY="ca.key"
CA_CRT="ca.crt"

echo "🔐 Generating SSL Certificates for Secure OTA Server"
echo "=================================================="

# Create certificates directory
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

# Generate CA private key
echo "📝 Generating CA private key..."
openssl genrsa -out "$CA_KEY" 4096

# Generate CA certificate
echo "📜 Generating CA certificate..."
openssl req -new -x509 -days 365 -key "$CA_KEY" -out "$CA_CRT" -subj "/C=US/ST=State/L=City/O=OTA-Dev/CN=OTA-CA"

# Generate server private key
echo "🔑 Generating server private key..."
openssl genrsa -out "$SERVER_KEY" 4096

# Generate server certificate signing request
echo "📋 Generating server certificate signing request..."
openssl req -new -key "$SERVER_KEY" -out server.csr -subj "/C=US/ST=State/L=City/O=OTA-Dev/CN=localhost"

# Generate server certificate signed by CA
echo "✅ Generating server certificate..."
openssl x509 -req -days 365 -in server.csr -CA "$CA_CRT" -CAkey "$CA_KEY" -CAcreateserial -out "$SERVER_CRT" -extensions v3_req -extfile <(
cat <<EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
C = US
ST = State
L = City
O = OTA-Dev
CN = localhost

[v3_req]
keyUsage = keyEncipherment, dataEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = ota-server
IP.1 = 127.0.0.1
IP.2 = 192.168.1.100
EOF
)

# Clean up CSR file
rm server.csr

# Set appropriate permissions
chmod 600 "$SERVER_KEY" "$CA_KEY"
chmod 644 "$SERVER_CRT" "$CA_CRT"

cd ..

echo "✅ SSL certificates generated successfully!"
echo ""
echo "Files created:"
echo "  📁 $CERT_DIR/$CA_CRT     - Certificate Authority (for ESP32)"
echo "  🔑 $CERT_DIR/$CA_KEY     - CA Private Key"
echo "  📜 $CERT_DIR/$SERVER_CRT - Server Certificate"
echo "  🔐 $CERT_DIR/$SERVER_KEY - Server Private Key"
echo ""
echo "💡 Next steps:"
echo "  1. Copy $CERT_DIR/$CA_CRT content to include/config.h"
echo "  2. Update your local IP in include/config.h"
echo "  3. Run './setup.sh' to start the OTA server"