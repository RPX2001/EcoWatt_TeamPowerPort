# Implementation Notes

## Simulation vs Production Architecture

### Current Implementation (Development/Testing)

The current system uses a **WiFi-based simulated inverter** for development and testing purposes.

#### Architecture

```
┌─────────────────────┐         WiFi/HTTP          ┌──────────────┐
│   ESP32 EcoWatt     │ ←─────────────────────→   │  Simulated   │
│                     │      Poll every 2s         │   Inverter   │
│  - WiFi Client      │                            │  (Software)  │
│  - HTTP Polling     │                            └──────────────┘
│  - Compression      │
│  - Security         │
└─────────────────────┘
```

#### Characteristics

**Communication:**
- WiFi-based HTTP polling
- Simulated register values
- No physical Modbus hardware required
- Rapid development and testing

**Power Management:**
- **Limited to peripheral gating only**
- Cannot use CPU frequency scaling (WiFi timing requirements)
- Cannot use sleep modes (continuous polling needed)
- WiFi radio always active

**Benefits:**
- Easy development setup
- No physical inverter required
- Rapid prototyping and testing
- Network-based debugging

**Limitations:**
- Higher power consumption
- Does not represent production power profile
- WiFi dependency

---

### Production Implementation (Deployment)

The production system will use **Modbus RTU over RS485** for communication with physical solar inverters.

#### Architecture

```
┌─────────────────────┐    Modbus RTU/RS485       ┌──────────────┐
│   ESP32 EcoWatt     │ ←─────────────────────→   │    Solar     │
│                     │      Poll every 2s         │   Inverter   │
│  - WiFi for Upload  │                            │  (Physical)  │
│  - Modbus Master    │                            └──────────────┘
│  - UART Interface   │
│  - RS485 Transceiver│
└─────────────────────┘
```

#### Characteristics

**Communication:**
- Modbus RTU protocol over RS485
- Physical UART with RS485 transceiver
- Standard industrial protocol
- 9600 baud (typical)

**Power Management - Full Suite:**

1. **Peripheral Gating** (Implemented)
   - RS485 transceiver power control
   - Enable only during Modbus transactions (~200ms)
   - Savings: 10-20mA continuous

2. **CPU Frequency Scaling** (Production)
   - 240MHz: WiFi transmission
   - 160MHz: Data processing and compression
   - 80MHz: Idle/waiting
   - Savings: 30-50mA during non-WiFi periods

3. **Light Sleep** (Production)
   - Sleep between Modbus polls
   - 1.8s sleep per 2s cycle (90% of time)
   - WiFi maintained in low-power state
   - Savings: 80-100mA during sleep

4. **Modem Sleep** (Production)
   - WiFi radio off between uploads
   - Upload every 15 seconds
   - Radio on for <2 seconds per upload
   - Savings: 60-80mA between uploads

5. **Deep Sleep** (Battery Operation)
   - For remote, battery-powered installations
   - Wake on timer (e.g., every 5 minutes)
   - Complete system shutdown except RTC
   - Savings: Reduces average to <1mA

**Power Profile Comparison:**

| Mode | Simulation (WiFi Poll) | Production (Modbus) |
|:-----|:----------------------|:--------------------|
| **Active** | 120-150mA | 120-150mA |
| **Processing** | 120-150mA | 80-100mA (freq scaling) |
| **Idle** | 100-120mA | 15-25mA (light sleep) |
| **Between Uploads** | 100-120mA | 20-30mA (modem sleep) |
| **Deep Sleep** | Not available | <1mA |
| **Average (AC)** | ~120mA | ~35mA |
| **Average (Battery)** | ~120mA | ~3mA (5min interval) |

---

## Configuration Differences

### Modbus Communication

**Simulation:**
```cpp
#define USE_SIMULATED_INVERTER true
#define SIMULATION_SERVER_IP "192.168.1.100"
#define SIMULATION_PORT 5001
```

**Production:**
```cpp
#define USE_SIMULATED_INVERTER false
#define MODBUS_UART_NUM UART_NUM_2
#define MODBUS_TX_PIN GPIO_NUM_17
#define MODBUS_RX_PIN GPIO_NUM_16
#define MODBUS_RTS_PIN GPIO_NUM_4  // RS485 DE/RE control
#define MODBUS_BAUD_RATE 9600
#define MODBUS_SLAVE_ID 1
```

### Power Management

**Simulation:**
```cpp
// Limited power management
#define ENABLE_PERIPHERAL_GATING true
#define ENABLE_FREQUENCY_SCALING false  // Breaks WiFi timing
#define ENABLE_LIGHT_SLEEP false        // Breaks polling
#define ENABLE_MODEM_SLEEP false        // Not beneficial
```

**Production:**
```cpp
// Full power management suite
#define ENABLE_PERIPHERAL_GATING true
#define ENABLE_FREQUENCY_SCALING true
#define ENABLE_LIGHT_SLEEP true
#define ENABLE_MODEM_SLEEP true
#define ENABLE_DEEP_SLEEP false  // true for battery operation
```

---

## Hardware Requirements

### Simulation Setup

**Required:**
- ESP32 development board
- USB cable for power and programming
- WiFi access point

**Optional:**
- None (purely software-based)

### Production Setup

**Required:**
- ESP32 development board
- RS485 transceiver module (e.g., MAX485, SP3485)
- Solar inverter with Modbus RTU support
- Shielded twisted pair cable (Cat5/Cat6)
- 120Ω termination resistors (for long cable runs)
- Power supply (or solar panel + battery)

**Wiring:**
```
ESP32                RS485 Module          Solar Inverter
GPIO_17 (TX) ─────→ DI                     
GPIO_16 (RX) ←───── RO                     
GPIO_4  (RTS) ─────→ DE/RE                
                     A ─────────────────→ A (Modbus+)
                     B ─────────────────→ B (Modbus-)
GND ──────────────── GND ─────────────── GND
```

---

## Testing Strategy

### Development Phase (Current)

**Using Simulation:**
- Algorithm development
- Compression testing
- Security implementation
- FOTA workflow
- Dashboard features
- API development

**Benefits:**
- Rapid iteration
- No hardware dependencies
- Controlled test scenarios
- Easy debugging

### Integration Phase (Next)

**Using Real Hardware:**
- Modbus protocol verification
- RS485 communication testing
- Physical inverter compatibility
- Power consumption validation
- Environmental testing (temperature, EMI)
- Long-term reliability testing

### Deployment Phase (Final)

**Field Testing:**
- Real solar installation
- Multi-day operation
- Battery life validation
- Network resilience
- Remote update verification
- Performance under load

---

## Migration Path

### Step 1: Hardware Setup

1. Add RS485 transceiver to ESP32
2. Connect to solar inverter
3. Verify Modbus communication
4. Test register reading

### Step 2: Firmware Updates

1. Change `USE_SIMULATED_INVERTER` to `false`
2. Configure Modbus parameters (baud, slave ID)
3. Enable production power management
4. Test power consumption

### Step 3: Validation

1. Verify data acquisition from real inverter
2. Confirm compression ratios with real data
3. Validate power management effectiveness
4. Test FOTA over cellular/WiFi

### Step 4: Deployment

1. Mount hardware in weather-proof enclosure
2. Install at solar site
3. Configure for local network/cellular
4. Enable remote monitoring

---

## Performance Expectations

### Compression Ratios

**Simulation (Synthetic Data):**
- More predictable patterns
- Higher compression possible
- Dictionary effectiveness: 80-85%

**Production (Real Inverter):**
- More variability in readings
- Environmental noise
- Dictionary effectiveness: 70-80%
- Delta encoding more beneficial

### Power Consumption

**Simulation:**
- ~120mA average
- ~3 Ah per day
- Unsuitable for battery operation

**Production:**
- ~35mA average (AC powered)
- ~3mA average (battery with deep sleep)
- ~75 mAh per day (battery mode)
- Suitable for solar panel + small battery

### Network Traffic

**Both Implementations:**
- Upload every 15 seconds (configurable)
- ~25-85 bytes per upload (compressed)
- ~150-500 bytes per upload (uncompressed fallback)
- ~5-30 KB per day

---

## Configuration Parameters

### Adjustable at Runtime

These parameters can be changed via remote configuration:

| Parameter | Default | Range | Description |
|:----------|:--------|:------|:------------|
| `poll_interval_ms` | 2000 | 1000-10000 | Modbus poll rate |
| `upload_interval_ms` | 15000 | 5000-60000 | Cloud upload rate |
| `compression_enabled` | true | true/false | Enable compression |
| `power_gating_enabled` | true | true/false | Peripheral power control |
| `security_level` | full | full/partial/none | Security features |

### Fixed at Compile Time

These parameters require firmware update to change:

| Parameter | Value | Description |
|:----------|:------|:------------|
| `RING_BUFFER_SIZE` | 7 | Samples buffered |
| `MAX_DICT_ENTRIES` | 16 | Compression dictionary size |
| `NONCE_NVS_KEY` | "nonce" | Anti-replay storage key |
| `MODBUS_BAUD_RATE` | 9600 | RS485 communication speed |

---

## Summary

The EcoWatt system is designed with a **simulation-first development approach** that enables rapid prototyping and testing without requiring physical hardware. The production deployment will leverage **Modbus RTU communication** and **comprehensive power management** for real-world solar inverter monitoring.

Key differences:
- **Communication**: WiFi polling → Modbus RTU
- **Power Management**: Basic gating → Full power optimization suite
- **Hardware**: Software only → RS485 transceiver + inverter
- **Power Consumption**: ~120mA → ~35mA (AC) or ~3mA (battery)

The architecture is designed to support both modes with minimal code changes, allowing for a smooth transition from development to production deployment.

[← Back to Documentation](README.md)
