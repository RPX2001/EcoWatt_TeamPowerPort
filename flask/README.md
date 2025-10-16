# EcoWatt Cloud API Server

A Flask-based cloud API server for processing compressed and secured solar inverter data from ESP32 devices.

## Overview

The EcoWatt Cloud API receives **secured and compressed** sensor data from ESP32 solar inverter monitoring devices. The server:
1. **Verifies security layer** (HMAC authentication + anti-replay protection)
2. **Decompresses data** using smart compression algorithms
3. **Processes register data** with human-readable conversions
4. **Publishes to MQTT** for real-time monitoring

## 🔐 Security Layer (NEW)

This server now includes **security layer verification** matching the ESP32 SecurityLayer implementation:

- ✅ **HMAC-SHA256 Authentication**: Verifies data integrity and authenticity
- ✅ **Anti-Replay Protection**: Nonce-based system prevents replay attacks
- ✅ **AES-128-CBC Decryption**: Supports encrypted payloads (when enabled)
- ✅ **Automatic Detection**: Handles both secured and unsecured payloads

**See detailed documentation:**
- [`SECURITY_LAYER_README.md`](SECURITY_LAYER_README.md) - Technical details
- [`SECURITY_FLOW_DIAGRAM.md`](SECURITY_FLOW_DIAGRAM.md) - Visual flow
- [`INTEGRATION_SUMMARY.md`](INTEGRATION_SUMMARY.md) - What changed

## Features

- **Security Layer**: HMAC verification, replay protection, AES decryption
- **Multi-format Decompression**: Dictionary+Bitmask, Temporal, Semantic, Bit-Packed
- **Batch Processing**: Handle 1-30 register values per compressed item
- **MQTT Integration**: Publishes data to HiveMQ broker
- **Register Mapping**: Solar inverter registers with units and conversions
- **OTA Updates**: Firmware management and updates
- **Performance Tracking**: Compression ratios and timing metrics
- **Health Monitoring**: Status and health check endpoints

## Quick Start

### 1. Install Dependencies

**Option A - Automatic:**
```bash
python install_dependencies.py
```

**Option B - Manual:**
```bash
pip install -r requirements.txt
```

Required packages:
- Flask (web framework)
- paho-mqtt (MQTT client)
- pycryptodome (AES/HMAC for security layer)
- requests (HTTP client)

### 2. Test Security Layer

```bash
python test_security.py
```

This creates a test secured payload to verify the security integration.

### 3. Run the Server

```bash
python flask_server_hivemq.py
```

Server starts on: `http://0.0.0.0:5001`

### 4. Test with Secured Data

```bash
curl -X POST http://localhost:5001/process \
     -H "Content-Type: application/json" \
     -d @test_secured_payload.json
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/process` | Process compressed sensor data (secured or unsecured) |
| GET | `/status` | Get server status and configuration |
| GET | `/health` | Health check endpoint |
| GET | `/changes` | Check for setting changes |
| POST | `/ota/check` | Check for firmware updates |
| GET | `/ota/download/:version` | Download firmware binary |

## Data Flow

```
ESP32 Device
    ↓ (1) Compress data
    ↓ (2) Apply security layer (HMAC + optional AES)
    ↓ (3) HTTP POST to /process
Flask Server
    ↓ (4) Detect security layer
    ↓ (5) Verify HMAC & nonce
    ↓ (6) Decrypt (if encrypted)
    ↓ (7) Extract original JSON
    ↓ (8) Decompress data
    ↓ (9) Process registers
    ↓ (10) Publish to MQTT
```

## Secured Payload Format

When ESP32 sends secured data, the payload looks like:

```json
{
  "nonce": 10001,
  "payload": "eyJkZXZpY2VfaWQiOi4uLn0=",
  "mac": "a1b2c3d4e5f6789abcdef...",
  "encrypted": false
}
```

The server automatically:
1. Detects the security layer (presence of `nonce`, `payload`, `mac`)
2. Verifies the nonce (must be > last valid nonce)
3. Decodes the base64 payload
4. Verifies the HMAC-SHA256
5. Extracts the original JSON for processing

## Register Map

| Register | Description | Unit | Example |
|----------|-------------|------|---------|
| REG_VAC1 | AC Voltage | 0.1V | 2400 (240.0V) |
| REG_IAC1 | AC Current | 0.01A | 170 (1.70A) |
| REG_IPV1 | PV Current 1 | 0.01A | 70 (0.70A) |
| REG_PAC | AC Power | 1W | 4000 (4000W) |
| REG_IPV2 | PV Current 2 | 0.01A | 65 (0.65A) |
| REG_TEMP | Temperature | 1°C | 550 (550°C) |

## Sample Request

```json
{
  "device_id": "ESP32_Solar_001",
  "compressed_data": [
    {
      "compressed_binary": "base64_encoded_compressed_data",
      "performance_metrics": {
        "method": "DICTIONARY",
        "academic_ratio": 0.500
      }
    }
  ]
}
```

## Sample MQTT Output

```json
{
  "register_data": {
    "REG_VAC1": 2400,
    "ac_voltage_readable": "240.0V",
    "REG_IAC1": 170,
    "ac_current_readable": "1.70A",
    "REG_PAC": 4000,
    "ac_power_readable": "4000W"
  }
}
```

## Configuration

### MQTT Settings
- **Broker**: `broker.hivemq.com:1883`
- **Topic**: `esp32/ecowatt_data`
- **QoS**: 0 (Fire and forget)

### Compression Support
- **Dictionary+Bitmask**: 5 predefined patterns with delta compression
- **Bit-Packed**: Variable bit-width encoding
- **Raw Binary**: Uncompressed 16-bit values
- **Batch Mode**: Up to 5 samples × 6 registers per payload

## Files

- `flask_server_hivemq.py` - Main Flask API server
- `API_DOCUMENTATION.md` - Complete API documentation
- `api_client_example.py` - Python client example and test suite
- `mqtt_subscriber_hivemq.py` - MQTT subscriber for monitoring output
- `README.md` - This file

## Monitoring

### Server Logs
The server provides structured logging for:
- Request processing statistics
- Decompression success rates
- MQTT publish status  
- Performance metrics
- Error conditions

### MQTT Monitoring
Use the included MQTT subscriber to monitor real-time output:

```bash
python mqtt_subscriber_hivemq.py
```

## Architecture

```
ESP32 Device → Flask API Server → MQTT Broker → Monitoring Dashboard
    ↓              ↓                    ↓
Compressed     Decompressed        Simplified
Sensor Data    Register Data      Register Data
```

## Compression Ratios

| Method | Typical Ratio | Use Case |
|--------|---------------|----------|
| Dictionary+Bitmask | 0.500 (50% reduction) | Standard operation |
| Bit-Packed | 0.600-0.800 | Small value ranges |
| Raw Binary | 1.000 (no compression) | Fallback/testing |

## Error Handling

The API handles various error conditions:
- Invalid compressed data formats
- Missing required fields
- Decompression failures
- MQTT connection issues
- Network timeouts

## Development

### Adding New Compression Methods
1. Implement decompression function in `flask_server_hivemq.py`
2. Add method ID detection in `decompress_smart_binary_data()`
3. Update documentation and examples

### Testing
The `api_client_example.py` includes comprehensive test cases:
- Single compressed data
- Batch compressed data
- Direct values (uncompressed)
- Error scenarios
- Performance timing

## License

This project is part of the EcoWatt solar monitoring system.