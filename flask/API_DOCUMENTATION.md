# EcoWatt Cloud API Documentation

## Overview

The EcoWatt Cloud API provides endpoints for processing solar inverter data from ESP32 devices. The API handles compressed sensor data decompression and publishes simplified register data to MQTT brokers.

**Base URL:** `http://10.40.99.2:5001`  
**MQTT Topic:** `esp32/ecowatt_data`  
**MQTT Broker:** `broker.hivemq.com:1883`

## Endpoints

### 1. POST /process

Primary endpoint for processing compressed sensor data from ESP32 devices.

#### Request Format

**Content-Type:** `application/json`

**Request Body Structure:**
```json
{
  "device_id": "string",           // Device identifier (optional, defaults to "ESP32_EcoWatt_Smart")
  "compressed_data": [             // Array of compressed data items
    {
      "compressed_binary": "string",    // Base64 encoded compressed data
      "performance_metrics": {          // Compression performance data
        "method": "string",             // Compression method used
        "academic_ratio": "number",     // Academic compression ratio
        "traditional_ratio": "number",  // Traditional compression ratio
        "compression_time_us": "number", // Compression time in microseconds
        "original_size": "number",      // Original data size
        "compressed_size": "number"     // Compressed data size
      },
      "decompression_metadata": {
        "method": "string",             // Decompression method
        "timestamp": "number"           // Unix timestamp in milliseconds
      }
    }
  ]
}
```

**Alternative Request Formats:**
- `smart_data`: Array of compressed items
- `binary_data`: Array of compressed items  
- `data`: Array of compressed items or direct values

#### Response Format

**Success Response (200):**
```json
{
  "status": "success",
  "device_id": "ESP32_EcoWatt_Smart",
  "compression_system": "Smart Selection v3.1 - Batch Support",
  "registers": ["REG_VAC1", "REG_IAC1", "REG_IPV1", "REG_PAC", "REG_IPV2", "REG_TEMP"],
  "processed_entries": 1,
  "total_samples_processed": 5,
  "decompression_successes": 1,
  "success_rate_percent": 100.0,
  "total_power_watts": 20000,
  "average_power_watts": 4000.0,
  "mqtt_topic": "esp32/ecowatt_data",
  "mqtt_published": true,
  "message": "Successfully processed 1 entries (5 samples) with 100.0% success rate"
}
```

**Error Response (400/500):**
```json
{
  "error": "Error description"
}
```

#### MQTT Payload Format

The API publishes simplified register data to MQTT:

```json
{
  "register_data": {
    "REG_VAC1": 2400,
    "ac_voltage_readable": "240.0V",
    "REG_IAC1": 170,
    "ac_current_readable": "1.70A",
    "REG_IPV1": 70,
    "pv_current_1_readable": "0.70A",
    "REG_PAC": 4000,
    "ac_power_readable": "4000W",
    "REG_IPV2": 65,
    "pv_current_2_readable": "0.65A",
    "REG_TEMP": 550,
    "temperature_readable": "550°C"
  }
}
```

### 2. GET /status

Returns current server status and configuration.

#### Response Format

```json
{
  "server_status": "running",
  "server_version": "Smart Selection Batch v3.1",
  "mqtt_connected": true,
  "mqtt_topic": "esp32/ecowatt_data",
  "supported_features": [
    "Dictionary+Bitmask batch compression (5 samples × 6 registers)",
    "Single sample compression",
    "Extended bitmask support for >16 values",
    "Multi-format ESP32 payload handling",
    "Academic ratio tracking (0.500 target achieved)"
  ],
  "current_time": "2025-09-28T14:30:25.123456"
}
```

### 3. GET /health

Health check endpoint for monitoring.

#### Response Format

```json
{
  "status": "healthy",
  "batch_support": true,
  "debug_enabled": true,
  "dictionary_patterns": 5
}
```

## Register Mapping

The EcoWatt system uses the following register definitions for solar inverter monitoring:

### Register Definitions

| Register | Name | Description | Unit | Scale Factor | Range |
|----------|------|-------------|------|--------------|--------|
| REG_VAC1 | AC Voltage | AC output voltage | 0.1V | ÷10 | 0-6553.5V |
| REG_IAC1 | AC Current | AC output current | 0.01A | ÷100 | 0-655.35A |
| REG_IPV1 | PV Current 1 | Solar panel 1 current | 0.01A | ÷100 | 0-655.35A |
| REG_PAC | AC Power | AC output power | 1W | ×1 | 0-65535W |
| REG_IPV2 | PV Current 2 | Solar panel 2 current | 0.01A | ÷100 | 0-655.35A |
| REG_TEMP | Temperature | Inverter temperature | 1°C | ×1 | 0-65535°C |

### Register Array Order

Registers are processed in the following fixed order:
```
[REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP]
[   0,        1,        2,        3,        4,        5   ]
```

### Sample Register Values

**Typical Operating Values:**
```json
{
  "REG_VAC1": 2400,    // 240.0V AC voltage
  "REG_IAC1": 170,     // 1.70A AC current  
  "REG_IPV1": 70,      // 0.70A PV current 1
  "REG_PAC": 4000,     // 4000W AC power
  "REG_IPV2": 65,      // 0.65A PV current 2
  "REG_TEMP": 550      // 550°C temperature
}
```

## Compression Support

### Supported Compression Methods

1. **Dictionary + Bitmask (0xD0)**
   - Uses predefined value patterns
   - Applies delta compression with bitmasks
   - Supports batch data (5 samples × 6 registers)
   - Target compression ratio: 0.500 (50% size reduction)

2. **Bit-Packed (0x01)**
   - Variable bit-width encoding
   - Suitable for small value ranges

3. **Raw Binary**
   - Uncompressed 16-bit values
   - Fallback format

### Dictionary Patterns

The system includes 5 predefined patterns optimized for typical solar inverter values:

```javascript
Pattern 0: [2400, 170, 70, 4000, 65, 550]   // Standard operation
Pattern 1: [2380, 100, 50, 2000, 25, 520]   // Low power
Pattern 2: [2450, 200, 90, 5000, 85, 580]   // High power  
Pattern 3: [2430, 165, 73, 4100, 65, 545]   // Nominal operation
Pattern 4: [2434, 179, 70, 4087, 72, 620]   // Peak operation
```

## Error Handling

### Common Error Responses

**No Compressed Data (400):**
```json
{
  "error": "No compressed data found in payload"
}
```

**Processing Error (500):**
```json
{
  "error": "Decompression failed: Invalid binary format"
}
```

### Error Scenarios

1. **Missing Data**: No compressed data arrays found in request
2. **Invalid Format**: Malformed base64 or binary data
3. **Decompression Failure**: Corrupted or unsupported compression format
4. **MQTT Connection**: Network issues with MQTT broker

## Usage Examples

### ESP32 Client Example

```javascript
// Sample ESP32 payload
{
  "device_id": "ESP32_Solar_001",
  "compressed_data": [
    {
      "compressed_binary": "0ABCDEF123456789...",
      "performance_metrics": {
        "method": "DICTIONARY",
        "academic_ratio": 0.500,
        "traditional_ratio": 2.0,
        "compression_time_us": 3655,
        "original_size": 12,
        "compressed_size": 6
      },
      "decompression_metadata": {
        "method": "DICTIONARY", 
        "timestamp": 1727538625000
      }
    }
  ]
}
```

### CURL Example

```bash
curl -X POST http://10.40.99.2:5001/process \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_Test",
    "compressed_data": [
      {
        "compressed_binary": "base64_encoded_data_here",
        "performance_metrics": {
          "method": "DICTIONARY",
          "academic_ratio": 0.5
        }
      }
    ]
  }'
```

## Rate Limits and Constraints

- **Request Size**: Maximum 10MB payload
- **Batch Size**: Up to 100 compressed data items per request
- **Sample Count**: Supports 1-30 register values per compressed item
- **Timeout**: 30 second request timeout
- **MQTT QoS**: QoS 0 (fire and forget)

## Security Considerations

- API currently operates without authentication
- Data transmitted in plain text over HTTP
- MQTT broker connection is unencrypted
- Rate limiting not implemented

## Monitoring and Logging

The API provides structured logging for:
- Request processing statistics
- Decompression success/failure rates
- MQTT publish status
- Performance metrics
- Error conditions

Log levels: INFO, WARNING, ERROR
