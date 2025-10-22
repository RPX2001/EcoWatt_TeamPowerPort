# M3 Integration Testing Guide

## Overview
This directory contains **real-world integration tests** that validate the complete M3 workflow with actual WiFi communication between ESP32 and Flask server.

## Test Structure

```
test/
├── unit_tests/                           # Unit tests (individual components)
│   ├── test_upload_packetizer.cpp
│   └── test_m4_command_execution.cpp
└── integration_tests/                    # Integration tests (end-to-end)
    └── test_m3_integration.cpp          # ESP32 real-world tests

flask/
└── tests/
    ├── unit_tests/                       # Flask unit tests
    │   └── test_m3_flask.py
    └── integration_tests/                # Flask integration tests
        └── test_m3_flask_integration.py
```

## ESP32 Integration Tests

### Configuration
The ESP32 integration test connects to WiFi and communicates with real inverter API and Flask server:

- **WiFi SSID**: `Galaxy A32B46A`
- **WiFi Password**: `aubz5724`
- **Inverter API**: `http://20.15.114.131:8080/api/inverter/read` (Real Modbus API)
- **Flask Server**: `http://192.168.242.249:5000` (Data aggregation)
- **Device ID**: `TEST_ESP32_INTEGRATION`
- **Test Samples**: 60 (1 minute of data for faster testing)
- **API Key**: Required for inverter API access (see API documentation)

### Test Coverage

1. **WiFi Connection** - Establishes connection and verifies network status
2. **Real Data Acquisition** - Fetches actual sensor data from real inverter via Modbus API (Registers 0-9)
3. **Complete Workflow** - End-to-end: acquire → buffer → compress → upload
4. **Compression Benchmarking** - Validates all 7 compression metrics with real inverter data
5. **Upload Retry Logic** - Tests failure handling and retry mechanism
6. **Lossless Compression** - Verifies data integrity through compress/decompress cycle
7. **Flask Server Health** - Checks server availability before tests
8. **Data Integrity** - Validates end-to-end data accuracy with real Modbus data

### Running ESP32 Integration Tests

#### Prerequisites
1. **Hardware**: Physical ESP32 board (not Wokwi simulation)
2. **Network**: Ensure ESP32 can reach WiFi network and internet
3. **Inverter API**: Requires valid API key (see API documentation)
4. **Flask Server**: Must be running at `192.168.242.249:5000`
5. **API Key**: Set in test code (obtain from EN4440 API Service Keys spreadsheet)

#### Steps

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/PIO/ECOWATT

# Run integration tests
pio test -e esp32dev --filter integration_tests --test-port /dev/ttyUSB0

# Or run all tests
pio test -e esp32dev --test-port /dev/ttyUSB0
```

#### Expected Output
```
=== Test: WiFi Connection ===
[WiFi] Connecting to: Galaxy A32B46A
[WiFi] Connected!
[WiFi] IP: 192.168.242.xxx
[PASS] WiFi connected successfully

=== Test: Real Data Acquisition ===
[Modbus] Reading registers 0-2 (Voltage, Current, Frequency)
[Modbus] Sent frame: 11030000000CC408
[Modbus] Response: 110318000015900000081E9B0015...
[Modbus] Parsed: Vac=230V, Iac=3.4A, Pac=782W
[PASS] Real data acquired from inverter

=== Test: Complete M3 Workflow ===
[Buffer] Acquired 60 samples from real inverter
[Compression] Voltage: 120 -> 38 (31.67%)
[Upload] Sending 256 bytes to Flask...
[Upload] Success! Response: {"status":"success"}
[PASS] Complete workflow succeeded!
```

## Flask Integration Tests

### Configuration
Flask integration tests validate server-side M3 functionality:

- **Test Client**: Flask test client (no network required)
- **Device ID**: `TEST_ESP32_INTEGRATION`
- **Compression Format**: Bit-packed with headers matching ESP32 format

### Test Coverage

1. **Flask Server Health** - Health endpoint validation
2. **Inverter Simulator** - Data generation endpoint
3. **Compressed Data Reception** - Accept and process ESP32 uploads
4. **Decompression Validation** - Verify decompression accuracy
5. **Compression Statistics** - Track all 7 benchmark metrics
6. **Multiple Device Support** - Handle concurrent device uploads
7. **Large Batch Upload** - Process 900-sample batches (15 min)
8. **Invalid Data Rejection** - Error handling and validation
9. **Concurrent Upload Handling** - Thread-safe data reception
10. **Data Persistence** - Storage and retrieval validation
11. **End-to-End Integrity** - Complete data accuracy verification
12. **Requirements Compliance** - Verify all M3 requirements met

### Running Flask Integration Tests

#### Prerequisites
1. **Python Environment**: Python 3.8+
2. **Dependencies**: Install from `flask/req.txt`

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask

# Install dependencies (if not already installed)
pip install -r req.txt

# Run integration tests
python -m pytest tests/integration_tests/test_m3_flask_integration.py -v

# Run with detailed output
python -m pytest tests/integration_tests/test_m3_flask_integration.py -v -s

# Run specific test
python -m pytest tests/integration_tests/test_m3_flask_integration.py::TestM3FlaskIntegration::test_receive_compressed_data_from_esp32 -v
```

#### Expected Output
```
tests/integration_tests/test_m3_flask_integration.py::TestM3FlaskIntegration::test_flask_server_health PASSED
tests/integration_tests/test_m3_flask_integration.py::TestM3FlaskIntegration::test_inverter_simulator_endpoint PASSED
tests/integration_tests/test_m3_flask_integration.py::TestM3FlaskIntegration::test_receive_compressed_data_from_esp32 PASSED
tests/integration_tests/test_m3_flask_integration.py::TestM3FlaskIntegration::test_compression_statistics_tracking PASSED
...

============== 12 passed in 2.45s ==============
```

## Combined Integration Testing

### Full System Test Procedure

1. **Start Flask Server**
   ```bash
   cd flask
   python flask_server_modular.py
   ```

2. **Run Flask Integration Tests** (in another terminal)
   ```bash
   cd flask
   python -m pytest tests/integration_tests/test_m3_flask_integration.py -v
   ```

3. **Upload ESP32 Integration Test** (on ESP32 hardware)
   ```bash
   cd PIO/ECOWATT
   pio test -e esp32dev --filter integration_tests --test-port /dev/ttyUSB0
   ```

4. **Monitor Results**
   - ESP32 Serial Monitor: Shows WiFi connection, data acquisition, upload
   - Flask Server Logs: Shows incoming requests, compression stats
   - Test Results: Both should show all tests passing

### Troubleshooting

#### ESP32 WiFi Connection Fails
- Verify WiFi credentials in test file
- Check WiFi network availability
- Ensure ESP32 is in range
- Check firewall settings

#### Inverter API Unreachable
- Verify API endpoint: `curl http://20.15.114.131:8080/api/inverter/read`
- Check API key is valid and set in code
- Ensure ESP32 has internet access (not just local network)
- Verify firewall allows port 8080
- Check API service status (may be down for maintenance)

#### Flask Server Unreachable
- Verify Flask server is running: `curl http://192.168.242.249:5000/health`
- Check server IP address: `ifconfig` or `ip addr`
- Ensure firewall allows port 5000
- Verify both ESP32 and server on same network

#### Modbus Frame Errors
- Check CRC calculation in Modbus frames
- Verify register addresses (0-9 valid, see API documentation)
- Test with Error Emulation API first to validate error handling
- Check for corrupt/delayed responses using Add Error Flag API

#### Upload Fails
- Check Flask server logs for errors
- Verify JSON payload format matches expectations
- Test with curl first:
  ```bash
  curl -X POST http://192.168.242.249:5000/api/aggregated_data \
    -H "Content-Type: application/json" \
    -d '{"device_id":"TEST","voltage_compressed":[1,2,3],"current_compressed":[1,2,3],"power_compressed":[1,2,3]}'
  ```

#### Compression Issues
- Check compression statistics: `curl http://192.168.242.249:5000/api/compression_stats`
- Verify data format matches bit-packing specification
- Test decompression locally with unit tests

## Next Steps

After M3 integration tests pass:

1. **Create M4 Unit Tests**
   - Command execution
   - Remote configuration
   - Security layer
   - FOTA (Firmware Over-The-Air)

2. **Create M4 Integration Tests**
   - End-to-end command flow
   - Secure communication
   - FOTA update process

3. **Performance Testing**
   - Load testing with multiple devices
   - Long-running stability tests
   - Network failure scenarios

## M3 Requirements Validated

✅ **Buffer**: Stores full 15-minute sample set (900 samples @ 1/sec)  
✅ **Compression**: Lightweight algorithm with 7-metric benchmarking  
✅ **Lossless**: Data integrity verified through compress/decompress cycle  
✅ **Upload**: 15-minute cycle with retry and chunking logic  
✅ **Flask API**: Accepts compressed data, stores, returns ACK  
✅ **Real-World**: Actual WiFi communication and HTTP protocol  
✅ **Multiple Devices**: Concurrent upload support  
✅ **Error Handling**: Retry logic and validation

## Support

For questions or issues:
- Check test output logs
- Review Flask server logs
- Examine ESP32 serial output
- Verify network connectivity with ping/curl
