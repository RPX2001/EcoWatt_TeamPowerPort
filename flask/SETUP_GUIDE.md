# Flask Server Setup Guide

## Quick Start

### 1. Install Dependencies

```powershell
# Navigate to flask directory
cd "d:\Work\Semester 7\Embedded Systems\EcoWatt_TeamPowerPort\flask"

# Install required packages
pip install -r requirements.txt
```

### 2. Run the Server

```powershell
python flask_server_hivemq.py
```

The server will start on `http://0.0.0.0:5001`

## Package Installation Details

The `requirements.txt` includes:
- **Flask**: Web framework for handling HTTP requests
- **paho-mqtt**: MQTT client for publishing to message broker
- **pycryptodome**: Cryptography library for AES/HMAC (security layer)
- **requests**: HTTP library for OTA firmware downloads

### Manual Installation (if needed)

```powershell
pip install Flask==2.3.3
pip install paho-mqtt==1.6.1
pip install pycryptodome==3.19.0
pip install requests==2.31.0
```

## Testing the Security Layer

### 1. Run Security Test

```powershell
python test_security.py
```

This will:
- Create a test secured payload
- Save it to `test_secured_payload.json`
- Display the payload structure

### 2. Test with Flask Server

Terminal 1 (Start server):
```powershell
python flask_server_hivemq.py
```

Terminal 2 (Send test request):
```powershell
curl -X POST http://localhost:5001/process -H "Content-Type: application/json" -d @test_secured_payload.json
```

## Features

### Security Layer
- ✓ HMAC-SHA256 authentication
- ✓ Anti-replay protection with nonce
- ✓ AES-128-CBC decryption support
- ✓ Automatic detection of secured vs unsecured payloads

### Data Processing
- ✓ Smart compression decompression
- ✓ Dictionary + Bitmask decompression
- ✓ Temporal pattern decompression
- ✓ Register data mapping

### MQTT Publishing
- ✓ Publishes to HiveMQ broker
- ✓ Topic: `esp32/ecowatt_data`

### OTA Updates
- ✓ Firmware version management
- ✓ HTTP-based firmware download

## Configuration

### Update Server IP
If you need to change the server IP address, update it in the ESP32 code:

```cpp
// In main.cpp
const char* dataPostURL = "http://YOUR_IP:5001/process";
const char* fetchChangesURL = "http://YOUR_IP:5001/changes";
```

### Update MQTT Broker
To use a different MQTT broker:

```python
# In flask_server_hivemq.py
MQTT_BROKER = "your.mqtt.broker.com"
MQTT_PORT = 1883
MQTT_TOPIC = "your/topic"
```

## Troubleshooting

### Import Errors
If you see "Import could not be resolved":
```powershell
pip install --upgrade pip
pip install -r requirements.txt
```

### Port Already in Use
If port 5001 is busy:
```python
# Change in flask_server_hivemq.py
app.run(host='0.0.0.0', port=5002, debug=False)  # Use different port
```

### Security Verification Fails
- Check that PSK keys match between ESP32 and Flask
- Verify nonce is incrementing (check ESP32 serial output)
- Ensure base64 encoding is correct

### MQTT Connection Issues
- Check if HiveMQ broker is accessible
- Verify network connectivity
- Try alternative broker: `test.mosquitto.org`

## Development Mode

For development with auto-reload:
```python
# In __main__ section
app.run(host='0.0.0.0', port=5001, debug=True)
```

## Production Deployment

For production, use a WSGI server like Gunicorn:

```powershell
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5001 flask_server_hivemq:app
```

## Logs

Server logs include:
- Security verification status
- Decompression results
- MQTT publish status
- OTA update checks

Enable debug logging:
```python
logging.basicConfig(level=logging.DEBUG)
```

## Next Steps

1. ✓ Install dependencies
2. ✓ Run test_security.py to verify security layer
3. ✓ Start Flask server
4. ✓ Upload ESP32 firmware
5. ✓ Monitor data flow

For more details, see `SECURITY_LAYER_README.md`
