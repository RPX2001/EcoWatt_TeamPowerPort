# EcoWatt Cloud API Server

A Flask-based cloud API server for processing compressed solar inverter data from ESP32 devices.

## Overview

The EcoWatt Cloud API receives compressed sensor data from ESP32 solar inverter monitoring devices, decompresses it using various algorithms, and publishes simplified register data to MQTT brokers for real-time monitoring.

## Features

- **Multi-format Decompression**: Dictionary+Bitmask, Bit-Packed, Raw Binary
- **Batch Processing**: Handle 1-30 register values per compressed item
- **MQTT Integration**: Publishes simplified data to HiveMQ broker
- **Register Mapping**: Solar inverter register definitions with units
- **Performance Tracking**: Compression ratio and timing metrics
- **Health Monitoring**: Status and health check endpoints

## Quick Start

### Prerequisites

```bash
pip install flask paho-mqtt
```

### Running the Server

```bash
python flask_server_hivemq.py
```

Server starts on: `http://10.40.99.2:5001`

### Testing the API

```bash
python api_client_example.py
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/process` | Process compressed sensor data |
| GET | `/status` | Get server status and configuration |
| GET | `/health` | Health check endpoint |

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