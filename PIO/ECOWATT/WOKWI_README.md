# Wokwi ESP32 Simulation Guide

## Overview
This project supports simulation using the Wokwi ESP32 simulator, allowing you to test the EcoWatt firmware without physical hardware.

## Setup

### Prerequisites
1. **VSCode Extension**: Install "Wokwi Simulator" from the VSCode marketplace
2. **PlatformIO**: Ensure PlatformIO is installed and configured
3. **License**: Wokwi requires a license for ESP32 simulation (free trial available)

### Files
- `wokwi.toml` - Wokwi configuration file (firmware paths, port forwarding)
- `diagram.json` - Circuit diagram (ESP32 + 2 LEDs + resistors)
- `include/wokwi_mock.h` - Mock HTTP/MQTT interface
- `src/wokwi_mock.cpp` - Mock implementation with simulated responses

## Building for Wokwi

### Using Justfile (Recommended)
```bash
# Build for Wokwi environment
just build-wokwi

# Or use full path from project root
cd PIO/ECOWATT && pio run -e wokwi
```

### Manual Build
```bash
cd PIO/ECOWATT
pio run -e wokwi
```

This builds with:
- `WOKWI_SIMULATION=1` flag (enables mock layer)
- `ENABLE_DIAGNOSTICS=1` flag (verbose logging)
- Mock WiFi credentials: `Wokwi-GUEST` (no password)

## Running the Simulation

### Method 1: VSCode Extension
1. Open `PIO/ECOWATT` folder in VSCode
2. Press `F1` → "Wokwi: Start Simulator"
3. Select the built firmware from `.pio/build/wokwi/firmware.elf`
4. Watch the Serial Monitor for output

### Method 2: CLI
```bash
wokwi-cli --firmware .pio/build/wokwi/firmware.elf --diagram diagram.json
```

### Method 3: Web Browser
1. Upload `diagram.json` to https://wokwi.com
2. Upload compiled firmware files
3. Click "Start Simulation"

## Mock Layer Features

### What's Mocked
The mock layer (`wokwi_mock.cpp/h`) provides simulated responses for:

**HTTP Endpoints:**
- `/diagnostics` - Returns success acknowledgment
- `/security/stats` - Returns simulated authentication statistics
- `/aggregation/stats` - Returns compression performance data
- `/fault/enable|disable|status|reset` - Fault injection control
- `/ota/check|chunk|verify|complete` - OTA update simulation
- `/command/queue|poll|result` - Remote command handling

**MQTT Operations:**
- `connect()` - Simulates broker connection
- `publish()` - Logs published messages
- `subscribe()` - Logs subscriptions
- `loop()` - No-op (no actual message processing)

**Sensor Readings:**
- Current: 0.5-5.0 A
- Voltage: 220-240 V
- Power: 100-1000 W
- Temperature: 20-35°C
- Frequency: 49.8-50.2 Hz
- Power Factor: 0.85-0.99

### How It Works
When `WOKWI_SIMULATION` is defined:
- All HTTP requests are intercepted and return mock JSON responses
- MQTT operations log to Serial but don't require actual broker
- Sensor readings are randomly generated with realistic values
- Network delays are eliminated for faster testing

## Testing Scenarios

### 1. Compression Algorithms
```cpp
// Tests run automatically in simulation
// Check Serial output for:
// - Dictionary + Bitmask (best for constant data)
// - Temporal Delta (best for linear trends)
// - Semantic RLE (best for repeated patterns)
// - Binary Auto-Select (chooses optimal method)
```

### 2. Security Features
```cpp
// Watch for:
// - RSA signature verification (mocked)
// - Nonce validation
// - CRC32 validation on decompression
// - Fault injection responses
```

### 3. Diagnostics Transmission
```cpp
// Observe:
// - Aggregation of register data
// - Compression selection and ratio
// - HTTP POST simulation to /diagnostics
// - Response handling
```

### 4. OTA Updates
```cpp
// Test:
// - Update availability check
// - Chunk download simulation
// - Signature verification
// - Update completion
```

## Circuit Diagram

The `diagram.json` includes:
- **ESP32-DEVKIT-V1** main board
- **Green LED** (D2) - Status indicator
- **Red LED** (D4) - Error indicator  
- **2x 220Ω resistors** for current limiting
- **Serial Monitor** connection (TX/RX)

## Port Forwarding

The `wokwi.toml` configures:
- Port 5000: Flask server communication (ESP32 → Host)
- This allows the simulated ESP32 to "communicate" with a local Flask server

## Conditional Compilation

Code that should only run in simulation:
```cpp
#ifdef WOKWI_SIMULATION
    // Use mock HTTP client
    mockHTTP.POST("/diagnostics", payload);
#else
    // Use real HTTP client
    http.POST(payload);
#endif
```

Code that should NOT run in simulation:
```cpp
#ifndef WOKWI_SIMULATION
    // Real hardware-specific code (ADC, I2C sensors, etc.)
    ads.readADC_SingleEnded(0);
#endif
```

## Troubleshooting

### Build Fails
```bash
# Clean build artifacts
just clean
# or
cd PIO/ECOWATT && pio run -t clean -e wokwi

# Rebuild
just build-wokwi
```

### Simulation Won't Start
1. Check `.pio/build/wokwi/firmware.elf` exists
2. Verify Wokwi license is valid
3. Ensure no other simulation is running
4. Check VSCode output panel for errors

### No Serial Output
1. Verify Serial Monitor is connected (TX/RX in diagram)
2. Check baud rate is set to 115200
3. Look for `[WOKWI SIMULATION MODE ACTIVE]` banner

### Mock Responses Not Working
1. Verify `WOKWI_SIMULATION` is defined in build flags
2. Check `wokwi_mock.cpp` is compiled (look for `[WOKWI MOCK]` prefix in Serial)
3. Ensure `initWokwiMocks()` is called in `setup()`

## Limitations

### What Works
- ✅ Core logic and algorithms
- ✅ Compression algorithms
- ✅ Security validation (signature, CRC)
- ✅ Diagnostics aggregation
- ✅ Serial output and logging
- ✅ LED indicators (visual feedback)

### What Doesn't Work
- ❌ Real WiFi connectivity (mocked)
- ❌ Real MQTT broker communication (mocked)
- ❌ Physical sensor readings (simulated)
- ❌ Hardware peripherals (ADC, I2C, SPI - unless Wokwi supports)
- ❌ Actual HTTP requests to Flask server (mocked)
- ❌ Real-time clock (RTC)

## Performance Notes

- Simulation runs at variable speed (not real-time)
- No network latency (instant mock responses)
- Random sensor values change each reading
- Good for logic testing, NOT for timing validation
- Use real hardware for production validation

## Integration with CI/CD

Wokwi can be integrated into automated testing:
```yaml
# Example GitHub Actions workflow
- name: Build for Wokwi
  run: |
    cd PIO/ECOWATT
    pio run -e wokwi

- name: Run Wokwi simulation
  run: |
    wokwi-cli --firmware .pio/build/wokwi/firmware.elf \
              --diagram diagram.json \
              --timeout 60
```

## Additional Resources

- [Wokwi Documentation](https://docs.wokwi.com/)
- [ESP32 Reference](https://docs.wokwi.com/parts/wokwi-esp32-devkit-v1)
- [PlatformIO Wokwi Integration](https://docs.platformio.org/en/latest/plus/wokwi.html)
- [EcoWatt Main README](../../README.md)

## Contact

For issues with Wokwi simulation, check:
1. Project issues on GitHub
2. Wokwi community forum
3. Team PowerPort documentation
