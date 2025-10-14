#!/bin/bash

# ESP32 Secure OTA Setup Script
# This script sets up the complete development environment for the ESP32 OTA system

set -e  # Exit on any error

echo "=============================================="
echo "ESP32 Secure OTA System Setup"
echo "=============================================="

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Python 3 is installed
check_python() {
    print_status "Checking Python installation..."
    if command -v python3 &> /dev/null; then
        PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
        print_success "Python $PYTHON_VERSION found"
    else
        print_error "Python 3 is required but not installed"
        exit 1
    fi
}

# Check if PlatformIO is installed
check_platformio() {
    print_status "Checking PlatformIO installation..."
    if command -v pio &> /dev/null; then
        PIO_VERSION=$(pio --version)
        print_success "PlatformIO found: $PIO_VERSION"
    else
        print_error "PlatformIO is required but not installed"
        print_status "Install PlatformIO with: pip install platformio"
        exit 1
    fi
}

# Setup Python virtual environment
setup_python_env() {
    print_status "Setting up Python virtual environment..."
    
    if [ ! -d "venv" ]; then
        python3 -m venv venv
        print_success "Virtual environment created"
    else
        print_warning "Virtual environment already exists"
    fi
    
    source venv/bin/activate
    
    print_status "Installing Python dependencies..."
    pip install --upgrade pip
    pip install -r requirements.txt
    
    print_success "Python dependencies installed"
}

# Generate SSL certificates
generate_certificates() {
    print_status "Generating SSL certificates..."
    
    if [ ! -f "server.crt" ] || [ ! -f "server.key" ]; then
        python3 -c "
import sys
sys.path.append('.')
from ota_server import generate_self_signed_cert
generate_self_signed_cert()
"
        print_success "SSL certificates generated"
    else
        print_warning "SSL certificates already exist"
    fi
}

# Create firmware directory structure
setup_directories() {
    print_status "Creating directory structure..."
    
    mkdir -p firmware
    mkdir -p uploads
    mkdir -p logs
    
    # Create initial firmware manifest
    cat > firmware/manifest.json << 'EOF'
{
  "current_version": "1.0.0",
  "versions": {},
  "force_update": false
}
EOF

    print_success "Directory structure created"
}

# Build initial ESP32 firmware
build_firmware() {
    print_status "Building initial ESP32 firmware..."
    
    # Update config.h with local settings
    if [ -f "include/config.h" ]; then
        print_status "Updating configuration..."
        
        # Get local IP address
        if command -v ipconfig &> /dev/null; then
            # macOS
            LOCAL_IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo "192.168.1.100")
        else
            # Linux
            LOCAL_IP=$(hostname -I | awk '{print $1}' 2>/dev/null || echo "192.168.1.100")
        fi
        
        print_status "Detected local IP: $LOCAL_IP"
        print_warning "Please update include/config.h with your WiFi credentials and server IP"
    fi
    
    # Build firmware
    if pio run; then
        print_success "ESP32 firmware built successfully"
        
        # Copy firmware to server directory
        if [ -f ".pio/build/esp32dev/firmware.bin" ]; then
            cp ".pio/build/esp32dev/firmware.bin" "firmware/1.0.0.bin"
            print_success "Firmware copied to server directory"
        fi
    else
        print_error "Failed to build ESP32 firmware"
        print_warning "Please check your PlatformIO configuration and try again"
    fi
}

# Create test scripts
create_test_scripts() {
    print_status "Creating test scripts..."
    
    # Create server start script
    cat > start_server.sh << 'EOF'
#!/bin/bash
echo "Starting ESP32 OTA Server..."
source venv/bin/activate
python3 ota_server.py
EOF
    chmod +x start_server.sh
    
    # Create firmware upload test script
    cat > test_upload.sh << 'EOF'
#!/bin/bash
# Test firmware upload
API_KEY="your-secure-api-key-here-change-this-in-production"
SERVER_URL="https://localhost:5000"

if [ -f "firmware/1.0.0.bin" ]; then
    echo "Uploading test firmware..."
    curl -k -X POST \
        -H "X-API-Key: $API_KEY" \
        -F "firmware=@firmware/1.0.0.bin" \
        -F "version=1.0.0" \
        "$SERVER_URL/admin/upload"
    echo
else
    echo "No firmware file found. Build firmware first."
fi
EOF
    chmod +x test_upload.sh
    
    # Create client test script
    cat > test_client.sh << 'EOF'
#!/bin/bash
# Test OTA client endpoints
API_KEY="your-secure-api-key-here-change-this-in-production"
SERVER_URL="https://localhost:5000"

echo "Testing OTA server endpoints..."

echo "1. Health check:"
curl -k "$SERVER_URL/health"
echo -e "\n"

echo "2. Get manifest:"
curl -k -H "X-API-Key: $API_KEY" -H "X-Device-ID: test-device" "$SERVER_URL/firmware/manifest"
echo -e "\n"

echo "3. Device status report:"
curl -k -X POST \
    -H "Content-Type: application/json" \
    -H "X-API-Key: $API_KEY" \
    -d '{"device_id":"test-device","status":"online","version":"1.0.0","message":"Test report"}' \
    "$SERVER_URL/firmware/report"
echo -e "\n"

echo "4. Get device list:"
curl -k -H "X-API-Key: $API_KEY" "$SERVER_URL/admin/devices"
echo -e "\n"
EOF
    chmod +x test_client.sh
    
    print_success "Test scripts created"
}

# Create configuration template
create_config_template() {
    print_status "Creating configuration template..."
    
    cat > config_template.txt << 'EOF'
==================================================
ESP32 OTA Configuration Template
==================================================

REQUIRED CONFIGURATIONS:

1. ESP32 Configuration (include/config.h):
   - Update WIFI_SSID with your WiFi network name
   - Update WIFI_PASSWORD with your WiFi password
   - Update OTA_SERVER_HOST with your server IP address
   - Update API_KEY (must match server configuration)
   - Update HMAC_KEY (must match server configuration)

2. Server Configuration (ota_server.py):
   - Update API_KEY (must match ESP32 configuration)
   - Update HMAC_KEY (must match ESP32 configuration)
   - Update HOST/PORT if needed
   - For production: Use proper SSL certificates

3. Network Configuration:
   - Ensure ESP32 and server are on the same network
   - Configure firewall to allow port 5000 (or your chosen port)
   - For remote access: Configure port forwarding

SECURITY NOTES:
- Change default API keys in production
- Use strong HMAC keys (32 random bytes)
- For production: Use CA-signed SSL certificates
- Enable additional security measures as needed

==================================================
EOF

    print_success "Configuration template created"
}

# Create README with instructions
create_readme() {
    print_status "Creating README with setup instructions..."
    
    cat > SETUP_README.md << 'EOF'
# ESP32 Secure OTA System Setup Guide

This guide will help you set up and test the complete ESP32 OTA system.

## Prerequisites

- Python 3.7 or later
- PlatformIO (install with: `pip install platformio`)
- ESP32 development board
- WiFi network access

## Quick Start

1. **Run the setup script:**
   ```bash
   chmod +x setup.sh
   ./setup.sh
   ```

2. **Configure your settings:**
   - Edit `include/config.h` with your WiFi credentials
   - Update server IP address in config.h
   - Review `config_template.txt` for all required settings

3. **Start the OTA server:**
   ```bash
   ./start_server.sh
   ```

4. **Build and upload ESP32 firmware:**
   ```bash
   pio run -t upload
   ```

5. **Test the system:**
   ```bash
   ./test_client.sh
   ```

## Manual Setup Steps

If the automatic setup fails, follow these manual steps:

### 1. Python Environment Setup
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 2. Generate SSL Certificates
```bash
python3 -c "from ota_server import generate_self_signed_cert; generate_self_signed_cert()"
```

### 3. Create Directories
```bash
mkdir -p firmware uploads logs
```

### 4. Build ESP32 Firmware
```bash
pio run
```

### 5. Start Server
```bash
python3 ota_server.py
```

## Configuration

### ESP32 Configuration (include/config.h)

Update these critical settings:

```cpp
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"
#define OTA_SERVER_HOST "192.168.1.100"  // Your server IP
#define API_KEY "your-secure-api-key-here-change-this-in-production"
```

### Server Configuration (ota_server.py)

Update the Config class:

```python
class Config:
    API_KEY = 'your-secure-api-key-here-change-this-in-production'
    HMAC_KEY = b'this-is-a-32-byte-hmac-key-change'  # Change this!
    HOST = '0.0.0.0'
    PORT = 5000
```

## Testing

### 1. Test Server Endpoints
```bash
./test_client.sh
```

### 2. Upload New Firmware
```bash
./test_upload.sh
```

### 3. Monitor ESP32 Serial Output
```bash
pio device monitor
```

## Firmware Update Process

1. **Build new firmware:**
   ```bash
   pio run
   ```

2. **Upload to server:**
   ```bash
   curl -k -X POST \
        -H "X-API-Key: your-api-key" \
        -F "firmware=@.pio/build/esp32dev/firmware.bin" \
        -F "version=1.0.1" \
        "https://localhost:5000/admin/upload"
   ```

3. **ESP32 will automatically detect and install the update**

## Security Considerations

### Development vs Production

**Development (current setup):**
- Self-signed SSL certificates
- Default API keys
- HTTP fallback option

**Production requirements:**
- CA-signed SSL certificates
- Strong, unique API keys
- Proper network security
- Secure key management
- Device authentication
- Update verification

### Security Checklist

- [ ] Change default API keys
- [ ] Use strong HMAC keys (32 random bytes)
- [ ] Deploy proper SSL certificates
- [ ] Configure firewall rules
- [ ] Enable logging and monitoring
- [ ] Implement device authentication
- [ ] Test rollback mechanisms
- [ ] Verify signature validation

## Troubleshooting

### Common Issues

1. **SSL Certificate Errors:**
   - Regenerate certificates: `rm server.* && python3 -c "from ota_server import generate_self_signed_cert; generate_self_signed_cert()"`
   - Check certificate in ESP32 code matches server certificate

2. **WiFi Connection Issues:**
   - Verify SSID and password in config.h
   - Check network connectivity
   - Monitor serial output for error messages

3. **API Authentication Errors:**
   - Ensure API keys match between ESP32 and server
   - Check HTTP headers are being sent correctly

4. **Firmware Download Failures:**
   - Verify firmware file exists on server
   - Check file permissions
   - Monitor server logs for errors

5. **OTA Update Failures:**
   - Ensure sufficient flash memory
   - Check partition table configuration
   - Verify firmware integrity (SHA256/HMAC)

### Debug Steps

1. **Check server logs:**
   ```bash
   tail -f ota_server.log
   ```

2. **Monitor ESP32 serial output:**
   ```bash
   pio device monitor
   ```

3. **Test server endpoints manually:**
   ```bash
   curl -k https://localhost:5000/health
   ```

4. **Verify network connectivity:**
   ```bash
   ping <esp32-ip-address>
   ```

## Advanced Configuration

### Custom Partition Scheme

Modify `partitions.csv` for your memory requirements:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x30000,
```

### Rollback Configuration

Adjust rollback timeout in config.h:

```cpp
#define ROLLBACK_TIMEOUT_MS 120000  // 2 minutes
```

### Update Intervals

Configure check frequency:

```cpp
#define CHECK_INTERVAL_MS 300000  // 5 minutes
```

## Support

For issues and questions:
1. Check the troubleshooting section
2. Review server and ESP32 logs
3. Verify configuration settings
4. Test with minimal setup first

## License

This project is provided as-is for educational and development purposes.
EOF

    print_success "README created"
}

# Main setup function
main() {
    print_status "Starting ESP32 OTA system setup..."
    
    # Check prerequisites
    check_python
    check_platformio
    
    # Setup environment
    setup_python_env
    setup_directories
    generate_certificates
    
    # Create support files
    create_test_scripts
    create_config_template
    create_readme
    
    # Build firmware
    build_firmware
    
    echo
    print_success "Setup completed successfully!"
    echo
    print_status "Next steps:"
    echo "1. Review and update include/config.h with your WiFi credentials"
    echo "2. Update server IP address in config.h"
    echo "3. Start the server: ./start_server.sh"
    echo "4. Upload firmware to ESP32: pio run -t upload"
    echo "5. Test the system: ./test_client.sh"
    echo
    print_warning "Don't forget to change the default API keys for production use!"
    echo
    print_status "For detailed instructions, see SETUP_README.md"
}

# Run main function
main