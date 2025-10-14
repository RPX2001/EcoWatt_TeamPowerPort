# ESP32 OTA Update System

A minimal Over-The-Air (OTA) firmware update system for ESP32 using PlatformIO and Flask.

## Overview

This system consists of:
- **ESP32 firmware** that connects to WiFi and checks for updates
- **Flask server** that hosts firmware binary files
- **Build tools** to compile and deploy firmware updates

## Prerequisites

- PlatformIO Core or PlatformIO IDE
- Python 3.7+
- ESP32 development board
- WiFi network

## Project Structure

```
├── src/
│   └── main.cpp              # ESP32 OTA firmware code
├── flask_server/
│   ├── app.py               # Flask server for hosting firmware
│   ├── requirements.txt     # Python dependencies
│   └── firmware/           # Directory for .bin files (created automatically)
├── platformio.ini          # PlatformIO configuration
├── copy_firmware.sh        # Script to copy compiled firmware
└── README.md              # This file
```

## Setup Instructions

### 1. Configure WiFi Credentials

Edit `src/main.cpp` and update your WiFi credentials:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverURL = "http://YOUR_COMPUTER_IP:5000";
```

### 2. Set Up Flask Server

```bash
# Navigate to flask_server directory
cd flask_server

# Install Python dependencies
pip install -r requirements.txt

# Start the Flask server
python app.py
```

The server will start on `http://localhost:5000` and create a `firmware/` directory automatically.

### 3. Build and Upload Initial Firmware

```bash
# Build the project
pio run

# Upload to ESP32 (first time via USB)
pio run --target upload

# Monitor serial output
pio device monitor
```

### 4. Deploy Firmware Update

After making changes to your code:

```bash
# Build new firmware
pio run

# Copy firmware to Flask server
./copy_firmware.sh

# The ESP32 will automatically check for updates every 30 seconds
```

## Hardware Wiring

No additional wiring is required beyond the basic ESP32 setup:

- **Power**: Connect ESP32 to USB or external 3.3V/5V power supply
- **Programming**: Use USB cable for initial firmware upload
- **WiFi**: Built-in ESP32 WiFi module (no external components needed)

### Basic ESP32 Connection:
```
ESP32 Board
├── USB Cable (for programming & power)
├── Built-in WiFi antenna
└── Serial monitor via USB
```

## How It Works

1. **ESP32 Boot**: Connects to WiFi and displays current firmware version
2. **Update Check**: Every 30 seconds, requests `/firmware/latest.bin` from Flask server
3. **Version Comparison**: Compares server version with current firmware version
4. **OTA Update**: If newer version available, downloads and installs firmware
5. **Reboot**: Automatically restarts with new firmware

## API Endpoints

### Flask Server

- `GET /` - Server status and info
- `GET /firmware/latest.bin` - Download latest firmware binary
- `HEAD /firmware/latest.bin` - Get firmware metadata (version, size)

## Serial Monitor Output

```
WiFi connected!
IP address: 192.168.1.100
Current firmware version: 1.0.0
Checking for updates...
Server firmware version: 1.0.1
New firmware available! Starting OTA update...
OTA update successful! Rebooting...
```

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password in `main.cpp`
- Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Ensure ESP32 is within WiFi range

### Server Connection Issues
- Verify Flask server is running (`python app.py`)
- Check computer firewall settings
- Ensure ESP32 and computer are on same network
- Update `serverURL` in `main.cpp` with correct IP address

### OTA Update Fails
- Check firmware file exists in `flask_server/firmware/`
- Verify firmware file is not corrupted
- Ensure sufficient flash memory on ESP32
- Monitor serial output for detailed error messages

### Get Your Computer's IP Address

**Windows:**
```cmd
ipconfig
```

**macOS/Linux:**
```bash
ifconfig
# or
ip addr show
```

Update the `serverURL` in `main.cpp` with your computer's IP address.

## Security Notes

⚠️ **This is a minimal implementation without security features:**

- No authentication or authorization
- No firmware signature verification  
- No encryption of firmware transfers
- No secure boot validation

**For production use, implement:**
- HTTPS/TLS encryption
- Firmware digital signatures
- Authentication tokens
- Rollback protection
- Secure boot verification

## Version Updates

To increment firmware version:

1. Edit `FIRMWARE_VERSION` in `src/main.cpp`
2. Build and deploy: `pio run && ./copy_firmware.sh`
3. ESP32 will detect and install the update automatically

## Development Workflow

1. **Code Changes**: Modify `src/main.cpp`
2. **Build**: `pio run`
3. **Deploy**: `./copy_firmware.sh`
4. **Test**: Monitor ESP32 serial output for update process
5. **Verify**: Confirm new version is running

## File Permissions

Make sure the copy script is executable:
```bash
chmod +x copy_firmware.sh
```

## Dependencies

### PlatformIO Libraries
- ESP32 Arduino Framework
- WiFi (built-in)
- HTTPClient (built-in)
- Update (built-in)

### Python Packages
- Flask
- Werkzeug

All dependencies are automatically managed by PlatformIO and pip.