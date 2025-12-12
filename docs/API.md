# 📡 EcoWatt API Reference

Complete REST API documentation for the EcoWatt backend server.

---

## Table of Contents

- [Base URL](#base-url)
- [Authentication](#authentication)
- [Device Management](#device-management)
- [Data Upload](#data-upload)
- [Power Data](#power-data)
- [Commands](#commands)
- [OTA Updates](#ota-updates)
- [Diagnostics](#diagnostics)
- [Security](#security)

---

## Base URL

```
Development: http://localhost:5001/api
Production:  https://your-domain.com:5001/api
```

---

## Authentication

Most endpoints require HMAC-SHA256 authentication for ESP32 devices.

### Headers

```http
Content-Type: application/json
X-Device-ID: ESP32_001
```

---

## Device Management

### Register Device

**POST** `/devices/register`

Register a new ESP32 device in the system.

**Request**:
```json
{
  "device_id": "ESP32_001",
  "firmware_version": "1.3.5",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "ip_address": "192.168.1.150"
}
```

**Response** (200):
```json
{
  "status": "success",
  "message": "Device registered successfully",
  "device_id": "ESP32_001"
}
```

---

### Get Device Info

**GET** `/devices/{device_id}`

Retrieve information about a specific device.

**Response** (200):
```json
{
  "device_id": "ESP32_001",
  "status": "online",
  "last_seen": "2024-12-12T10:30:45Z",
  "firmware_version": "1.3.5",
  "wifi_rssi": -45,
  "uptime_seconds": 86400,
  "total_uploads": 5760,
  "last_nonce": 15432
}
```

---

### List All Devices

**GET** `/devices`

Get list of all registered devices.

**Query Parameters**:
- `status` (optional): Filter by status (`online`, `offline`)
- `limit` (optional): Number of results (default: 50)
- `offset` (optional): Pagination offset (default: 0)

**Response** (200):
```json
{
  "devices": [
    {
      "device_id": "ESP32_001",
      "status": "online",
      "last_seen": "2024-12-12T10:30:45Z",
      "firmware_version": "1.3.5"
    },
    {
      "device_id": "ESP32_002",
      "status": "offline",
      "last_seen": "2024-12-11T22:15:30Z",
      "firmware_version": "1.3.4"
    }
  ],
  "total": 2,
  "limit": 50,
  "offset": 0
}
```

---

### Update Device Status

**PUT** `/devices/{device_id}/status`

Update device status information.

**Request**:
```json
{
  "wifi_rssi": -50,
  "uptime_seconds": 90000,
  "free_heap": 180000,
  "cpu_frequency": 240
}
```

**Response** (200):
```json
{
  "status": "success",
  "message": "Device status updated"
}
```

---

## Data Upload

### Upload Sensor Data

**POST** `/upload/data`

Upload compressed and secured sensor data from ESP32.

**Request**:
```json
{
  "device_id": "ESP32_001",
  "nonce": 15432,
  "payload": "0xD0014502A3...",
  "mac": "a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4e6f8a0b2c4d6e8f0a2b4",
  "compressed": true,
  "compression_type": "dictionary",
  "encrypted": false,
  "timestamp": "2024-12-12T10:30:45Z"
}
```

**Response** (200):
```json
{
  "status": "success",
  "message": "Data uploaded successfully",
  "samples_received": 7,
  "compression_ratio": 96.4,
  "nonce_updated": 15432
}
```

**Error Response** (403):
```json
{
  "status": "error",
  "error_code": "REPLAY_ATTACK",
  "message": "Nonce validation failed: replay attack detected",
  "received_nonce": 15430,
  "expected_nonce": 15432
}
```

---

### Upload Diagnostics

**POST** `/upload/diagnostics`

Upload diagnostic data for system monitoring.

**Request**:
```json
{
  "device_id": "ESP32_001",
  "diagnostics": {
    "compression_stats": {
      "dictionary_count": 120,
      "delta_count": 45,
      "rle_count": 15,
      "bitpack_count": 10
    },
    "power_stats": {
      "uart_on_time_ms": 38400,
      "uart_off_time_ms": 345600,
      "duty_cycle_percent": 10.0
    },
    "network_stats": {
      "packets_sent": 192,
      "packets_failed": 2,
      "average_latency_ms": 85
    },
    "errors": {
      "modbus_timeouts": 3,
      "upload_failures": 2,
      "hmac_failures": 0
    }
  }
}
```

**Response** (200):
```json
{
  "status": "success",
  "message": "Diagnostics uploaded successfully"
}
```

---

## Power Data

### Get Latest Data

**GET** `/power/latest/{device_id}`

Get the most recent power data for a device.

**Response** (200):
```json
{
  "device_id": "ESP32_001",
  "timestamp": "2024-12-12T10:30:45Z",
  "data": {
    "grid_voltage_v": 220.5,
    "grid_current_a": 5.2,
    "grid_power_w": 1146.6,
    "grid_frequency_hz": 50.0,
    "pv_voltage_v": 380.0,
    "pv_current_a": 8.5,
    "pv_power_w": 3230.0,
    "temperature_c": 25.3,
    "inverter_status": "normal",
    "daily_energy_kwh": 18.5
  }
}
```

---

### Get Historical Data

**GET** `/power/history/{device_id}`

Retrieve historical power data.

**Query Parameters**:
- `start` (required): Start timestamp (ISO 8601)
- `end` (required): End timestamp (ISO 8601)
- `interval` (optional): Data interval (`1m`, `5m`, `15m`, `1h`, default: `15m`)

**Example**:
```
GET /power/history/ESP32_001?start=2024-12-12T00:00:00Z&end=2024-12-12T23:59:59Z&interval=1h
```

**Response** (200):
```json
{
  "device_id": "ESP32_001",
  "start": "2024-12-12T00:00:00Z",
  "end": "2024-12-12T23:59:59Z",
  "interval": "1h",
  "data": [
    {
      "timestamp": "2024-12-12T00:00:00Z",
      "avg_grid_power_w": 0,
      "avg_pv_power_w": 0,
      "energy_kwh": 0
    },
    {
      "timestamp": "2024-12-12T08:00:00Z",
      "avg_grid_power_w": 2500,
      "avg_pv_power_w": 3200,
      "energy_kwh": 3.2
    }
  ],
  "total_records": 24
}
```

---

### Get Aggregated Data

**GET** `/power/aggregation/{device_id}`

Get aggregated power statistics.

**Query Parameters**:
- `period` (required): `daily`, `weekly`, `monthly`, `yearly`

**Response** (200):
```json
{
  "device_id": "ESP32_001",
  "period": "daily",
  "data": {
    "total_energy_kwh": 45.8,
    "peak_power_w": 4500,
    "avg_power_w": 1910,
    "runtime_hours": 12.5,
    "efficiency_percent": 94.2
  }
}
```

---

## Commands

### Queue Command

**POST** `/command/queue`

Queue a command for ESP32 execution.

**Request - Modbus Write**:
```json
{
  "device_id": "ESP32_001",
  "command_type": "modbus_write",
  "parameters": {
    "register": 40001,
    "value": 2000,
    "description": "Set power limit to 2000W"
  },
  "timeout": 10,
  "priority": "normal"
}
```

**Request - Configuration Update**:
```json
{
  "device_id": "ESP32_001",
  "command_type": "config_update",
  "parameters": {
    "poll_interval_ms": 5000,
    "upload_interval_ms": 30000,
    "compression_enabled": true
  },
  "timeout": 5
}
```

**Response** (200):
```json
{
  "status": "success",
  "command_id": "cmd_abc123",
  "message": "Command queued successfully",
  "estimated_execution": "2024-12-12T10:31:00Z"
}
```

---

### Get Pending Commands

**GET** `/command/pending/{device_id}`

ESP32 polls this endpoint to retrieve pending commands.

**Response** (200):
```json
{
  "commands": [
    {
      "command_id": "cmd_abc123",
      "command_type": "modbus_write",
      "parameters": {
        "register": 40001,
        "value": 2000
      },
      "timeout": 10,
      "created_at": "2024-12-12T10:30:50Z"
    }
  ],
  "count": 1
}
```

**Response** (204):
```
No pending commands
```

---

### Report Command Status

**POST** `/command/status`

ESP32 reports command execution result.

**Request - Success**:
```json
{
  "command_id": "cmd_abc123",
  "device_id": "ESP32_001",
  "status": "success",
  "execution_time_ms": 247,
  "result": {
    "modbus_response_code": "0x06",
    "new_value": 2000
  }
}
```

**Request - Failure**:
```json
{
  "command_id": "cmd_abc123",
  "device_id": "ESP32_001",
  "status": "failed",
  "error_code": "MODBUS_TIMEOUT",
  "error_message": "Inverter did not respond within 5 seconds",
  "retry_count": 3
}
```

**Response** (200):
```json
{
  "status": "success",
  "message": "Command status updated"
}
```

---

### Get Command History

**GET** `/command/history/{device_id}`

Retrieve command execution history.

**Query Parameters**:
- `limit` (optional): Number of results (default: 50)
- `status` (optional): Filter by status (`success`, `failed`, `pending`)

**Response** (200):
```json
{
  "device_id": "ESP32_001",
  "commands": [
    {
      "command_id": "cmd_abc123",
      "command_type": "modbus_write",
      "status": "success",
      "created_at": "2024-12-12T10:30:50Z",
      "executed_at": "2024-12-12T10:31:02Z",
      "execution_time_ms": 247
    }
  ],
  "total": 125,
  "limit": 50
}
```

---

## OTA Updates

### Check for Updates

**GET** `/ota/check/{device_id}`

ESP32 checks if firmware update is available.

**Response** (200) - Update Available:
```json
{
  "update_available": true,
  "firmware_version": "1.3.6",
  "file_size": 1024000,
  "manifest_url": "/ota/manifest/1.3.6",
  "release_notes": "Bug fixes and performance improvements"
}
```

**Response** (200) - No Update:
```json
{
  "update_available": false,
  "current_version": "1.3.5"
}
```

---

### Get Firmware Manifest

**GET** `/ota/manifest/{version}`

Retrieve firmware update manifest.

**Response** (200):
```json
{
  "version": "1.3.6",
  "file_name": "firmware_1.3.6.bin",
  "file_size": 1024000,
  "chunk_size": 2048,
  "total_chunks": 500,
  "sha256_hash": "a1b2c3d4e5f6...",
  "rsa_signature": "1a2b3c4d5e6f...",
  "aes_iv": "0102030405060708090a0b0c0d0e0f10",
  "created_at": "2024-12-12T09:00:00Z"
}
```

---

### Download Firmware Chunk

**GET** `/ota/download/{version}/{chunk_number}`

Download specific firmware chunk (2KB).

**Response** (200):
```
Content-Type: application/octet-stream
Content-Length: 2048

<binary data>
```

**Response** (404):
```json
{
  "status": "error",
  "message": "Chunk not found"
}
```

---

### Report OTA Status

**POST** `/ota/status`

ESP32 reports OTA update progress/result.

**Request - In Progress**:
```json
{
  "device_id": "ESP32_001",
  "firmware_version": "1.3.6",
  "status": "downloading",
  "progress_percent": 45.2,
  "chunks_downloaded": 226,
  "total_chunks": 500
}
```

**Request - Success**:
```json
{
  "device_id": "ESP32_001",
  "firmware_version": "1.3.6",
  "status": "success",
  "old_version": "1.3.5",
  "boot_time_ms": 3200
}
```

**Request - Failure**:
```json
{
  "device_id": "ESP32_001",
  "firmware_version": "1.3.6",
  "status": "failed",
  "error_code": "SIGNATURE_INVALID",
  "error_message": "RSA signature verification failed",
  "rolled_back_to": "1.3.5"
}
```

**Response** (200):
```json
{
  "status": "success",
  "message": "OTA status updated"
}
```

---

## Diagnostics

### Get System Diagnostics

**GET** `/diagnostics/{device_id}`

Retrieve comprehensive diagnostic information.

**Response** (200):
```json
{
  "device_id": "ESP32_001",
  "timestamp": "2024-12-12T10:30:45Z",
  "system": {
    "uptime_seconds": 86400,
    "free_heap_bytes": 180000,
    "min_free_heap_bytes": 150000,
    "cpu_frequency_mhz": 240
  },
  "compression": {
    "total_compressions": 5760,
    "avg_compression_ratio": 94.2,
    "method_distribution": {
      "dictionary": 65,
      "delta": 25,
      "rle": 8,
      "bitpack": 2
    }
  },
  "network": {
    "wifi_rssi": -45,
    "wifi_reconnects": 2,
    "packets_sent": 5760,
    "packets_failed": 12,
    "avg_latency_ms": 85
  },
  "modbus": {
    "total_polls": 43200,
    "timeouts": 15,
    "crc_errors": 3,
    "avg_poll_time_ms": 180
  },
  "power": {
    "uart_duty_cycle_percent": 10.0,
    "estimated_power_savings_ma": 15.0
  },
  "errors": {
    "total_errors": 30,
    "recent_errors": [
      {
        "timestamp": "2024-12-12T09:15:30Z",
        "type": "MODBUS_TIMEOUT",
        "message": "Modbus read timeout on register 40001"
      }
    ]
  }
}
```

---

## Security

### Validate Packet

**POST** `/security/validate`

Validate packet security (internal use by upload handlers).

**Request**:
```json
{
  "device_id": "ESP32_001",
  "nonce": 15432,
  "payload": "base64_data",
  "mac": "hmac_signature"
}
```

**Response** (200):
```json
{
  "valid": true,
  "nonce_valid": true,
  "hmac_valid": true
}
```

**Response** (403):
```json
{
  "valid": false,
  "nonce_valid": false,
  "hmac_valid": true,
  "error": "Replay attack detected"
}
```

---

## Error Codes

| Code | Description |
|:-----|:------------|
| `REPLAY_ATTACK` | Nonce validation failed |
| `INVALID_HMAC` | HMAC signature mismatch |
| `DEVICE_NOT_FOUND` | Device ID not registered |
| `INVALID_PAYLOAD` | Malformed payload data |
| `DECOMPRESSION_FAILED` | Decompression error |
| `MODBUS_TIMEOUT` | Modbus communication timeout |
| `SIGNATURE_INVALID` | RSA signature verification failed |
| `HASH_MISMATCH` | Firmware hash doesn't match |
| `COMMAND_TIMEOUT` | Command execution timeout |
| `RATE_LIMIT_EXCEEDED` | Too many requests |

---

## Rate Limiting

All endpoints are rate-limited to prevent abuse:

- **Upload endpoints**: 10 requests/minute per device
- **Command endpoints**: 30 requests/minute per device
- **Query endpoints**: 100 requests/minute per IP

**Rate Limit Response** (429):
```json
{
  "status": "error",
  "error_code": "RATE_LIMIT_EXCEEDED",
  "message": "Too many requests",
  "retry_after_seconds": 60
}
```

---

[← Back to Main README](../README.md)
