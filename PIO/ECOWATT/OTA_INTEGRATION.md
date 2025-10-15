# EcoWatt FOTA Integration Guide

## Overview
This guide explains how to integrate the OTA Manager into your main EcoWatt system.

## Integration Steps

### 1. Add Include to Main Application

In your main application file (e.g., `src/main.cpp`), add:

```cpp
#include "application/OTAManager.h"

// Global OTA manager instance
OTAManager otaManager("1.0.3"); // Current firmware version
```

### 2. Initialize in Setup Function

```cpp
void setup() {
    // ... existing setup code ...
    
    // Initialize OTA Manager
    otaManager.begin();
    
    // ... rest of setup ...
}
```

### 3. Add Timer-based OTA Checks

Add this to your main loop or use Timer 3 as specified:

```cpp
// Timer-based OTA check (every 30 minutes)
unsigned long lastOTACheck = 0;
const unsigned long OTA_CHECK_INTERVAL = 30 * 60 * 1000; // 30 minutes

void loop() {
    // ... existing loop code ...
    
    // Periodic OTA check
    if (millis() - lastOTACheck > OTA_CHECK_INTERVAL) {
        if (otaManager.getState() == OTA_IDLE) {
            Serial.println("Checking for firmware updates...");
            otaManager.checkForUpdate();
        }
        lastOTACheck = millis();
    }
    
    // ... rest of loop ...
}
```

### 4. Post-Boot Diagnostics

Add this after WiFi initialization in setup():

```cpp
void setup() {
    // ... WiFi initialization ...
    
    // Run post-boot diagnostics (validates new firmware)
    if (otaManager.runDiagnostics()) {
        Serial.println("✓ Post-boot diagnostics passed");
    } else {
        Serial.println("✗ Post-boot diagnostics failed - initiating rollback");
        otaManager.handleRollback();
    }
    
    // ... rest of setup ...
}
```

## Configuration Requirements

### 1. PlatformIO Dependencies

Add to `platformio.ini`:

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    ArduinoJson
    WiFi
    HTTPClient
    bblanchon/ArduinoJson@^7.0.0

build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DMBEDTLS_CONFIG_FILE="mbedtls_config.h"

# Enable OTA partition scheme
board_build.partitions = min_spiffs.csv
```

### 2. Partition Table

Create `partitions.csv` (or use existing):

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1A0000,
app1,     app,  ota_1,   0x1B0000,0x1A0000,
spiffs,   data, spiffs,  0x350000,0xB0000,
```

## Usage Examples

### Manual OTA Check

```cpp
// Trigger manual OTA check
if (digitalRead(UPDATE_BUTTON) == LOW) {
    otaManager.checkForUpdate();
}
```

### Status Monitoring

```cpp
// Monitor OTA status
OTAState state = otaManager.getState();
OTAProgress progress = otaManager.getProgress();

if (state == OTA_DOWNLOADING) {
    float percentage = (float)progress.chunks_received / progress.total_chunks * 100.0;
    Serial.printf("Download: %.1f%%\n", percentage);
}
```

### Error Handling

```cpp
// Handle OTA errors
if (otaManager.getState() == OTA_ERROR) {
    Serial.println("OTA failed - resetting");
    otaManager.reset();
}
```

## Security Notes

1. **Key Storage**: Security keys are embedded in `keys.h` - protect this file
2. **HTTPS**: Server should use HTTPS in production
3. **Validation**: All firmware is cryptographically verified before installation
4. **Rollback**: Automatic rollback if new firmware fails diagnostics

## Testing Procedure

1. **Prepare Test Firmware**: Create new firmware version (increment version number)
2. **Server Preparation**: Use Flask server to prepare and sign firmware
3. **Deploy**: Copy firmware to server and start Flask server
4. **Trigger Update**: Call `otaManager.checkForUpdate()` or wait for timer
5. **Monitor**: Watch serial output for download progress and verification
6. **Validate**: Ensure diagnostics pass after reboot

## Troubleshooting

### Common Issues:

1. **"No update available"**: Check version numbers and server availability
2. **"Signature verification failed"**: Ensure keys match between server and device
3. **"Download failed"**: Check network connectivity and server status
4. **"Rollback triggered"**: New firmware failed diagnostics - check compatibility

### Debug Outputs:

- Enable verbose logging in PlatformIO: `build_flags = -DCORE_DEBUG_LEVEL=5`
- Monitor serial output during OTA process
- Check server logs for HTTP request/response details

## Integration Checklist

- [ ] OTAManager files added to project
- [ ] Security keys copied from server
- [ ] Dependencies added to platformio.ini
- [ ] Partition table configured
- [ ] OTA manager initialized in setup()
- [ ] Periodic checks added to main loop
- [ ] Post-boot diagnostics integrated
- [ ] Error handling implemented
- [ ] Testing completed

## Performance Impact

- **Memory Usage**: ~8KB RAM for buffers and crypto contexts
- **Flash Usage**: ~50KB for mbedtls libraries and OTA code
- **Network Usage**: Chunked downloads minimize memory requirements
- **CPU Usage**: Crypto operations during download only

The OTA system is designed for minimal impact on normal operations.