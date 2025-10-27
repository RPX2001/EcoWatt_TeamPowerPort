# MQTT Subscriber for EcoWatt Data

Python script to subscribe to MQTT topics and receive decompressed sensor data from ESP32 devices.

## Files

- **`mqtt_subscriber.py`** - Main subscriber script to listen for data
- **`test_mqtt_publisher.py`** - Test script to simulate data publishing

## Installation

Install required dependency:
```bash
pip install paho-mqtt
```

## Usage

### Subscribe to All Devices
```bash
python3 mqtt_subscriber.py
```

### Subscribe to Specific Device
```bash
python3 mqtt_subscriber.py --device TEST_ESP32_INTEGRATION
```

### Use Different Broker
```bash
python3 mqtt_subscriber.py --broker localhost --port 1883
```

### Subscribe to Specific Device on Custom Broker
```bash
python3 mqtt_subscriber.py --device ESP32_001 --broker broker.hivemq.com
```

## Testing

To test the subscriber without running the full Flask server:

**Terminal 1 - Start Subscriber:**
```bash
python3 mqtt_subscriber.py
```

**Terminal 2 - Publish Test Data:**
```bash
python3 test_mqtt_publisher.py
```

You should see the subscriber receive and display the test data.

## MQTT Topic Structure

- **Pattern:** `ecowatt/data/{device_id}`
- **Examples:**
  - `ecowatt/data/TEST_ESP32_M3`
  - `ecowatt/data/ESP32_001`
  - `ecowatt/data/TEST_ESP32_INTEGRATION`

## Data Formats

### Decompressed Data Payload
```json
{
  "device_id": "TEST_ESP32_M3",
  "timestamp": 1698012345.678,
  "values": [5000, 5001, 5002, ...],
  "sample_count": 60,
  "compression_stats": {
    "method": "Bit Packing",
    "ratio": 0.65,
    "original_size": 120,
    "compressed_size": 78
  }
}
```

### Aggregated Data Payload
```json
{
  "device_id": "TEST_ESP32_M3",
  "timestamp": 1698012345.678,
  "samples": [
    {"voltage": 5000, "current": 500, "power": 2000, "timestamp": 1698012345000},
    {"voltage": 5001, "current": 501, "power": 2001, "timestamp": 1698012346000}
  ],
  "sample_count": 2,
  "type": "aggregated"
}
```

## Output Example

```
================================================================================
📨 Message #1 | Topic: ecowatt/data/TEST_ESP32_M3
🔌 Device: TEST_ESP32_M3 | Time: 2025-10-27 23:45:12
--------------------------------------------------------------------------------
📊 Type: Decompressed Data
📈 Samples: 60
🗜️  Compression Method: Bit Packing
🗜️  Compression Ratio: 65.00%
🗜️  Original Size: 120 bytes
🗜️  Compressed Size: 78 bytes

📋 Values (showing first 10 of 60):
   [0] = 5000.0
   [1] = 5001.0
   [2] = 5002.0
   ... (50 more values)

📊 Session Stats: 1 messages, 60 total samples
================================================================================
```

## Integration with Flask Server

When the Flask server receives data at `/aggregated/{device_id}`, it automatically:
1. Decompresses the data (if compressed)
2. Publishes to MQTT topic `ecowatt/data/{device_id}`
3. Any subscriber listening to that topic will receive the data in real-time

## Command Line Options

```
--broker    MQTT broker address (default: broker.hivemq.com)
--port      MQTT broker port (default: 1883)
--device    Specific device ID to filter (default: all devices)
```

## Notes

- The subscriber automatically reconnects if disconnected
- Press `Ctrl+C` to stop the subscriber gracefully
- Message counter and sample statistics are displayed
- Both compressed and aggregated data formats are supported
