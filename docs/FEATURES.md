# 🌟 EcoWatt Features Guide

This document provides in-depth explanations of all major features in the EcoWatt system.

---

## Table of Contents

- [Security Architecture](#security-architecture)
- [Compression System](#compression-system)
- [FOTA Updates](#fota-updates)
- [Power Management](#power-management)
- [Remote Control](#remote-control)
- [Web Dashboard](#web-dashboard)
- [Modbus Communication](#modbus-communication)

---

## 🔐 Security Architecture

### Multi-Layer Upload Protection

Every data packet uploaded from the ESP32 to the server passes through a **3-layer security pipeline**:

#### Layer 1: Anti-Replay Protection

**Problem**: An attacker could capture valid data packets and replay them to the server, potentially causing duplicate entries or masking real device failures.

**Solution**: Each packet includes a **monotonically increasing nonce** value:

```json
{
  "nonce": 10001,
  "payload": "...",
  "mac": "..."
}
```

- **Nonce Storage**: Stored in ESP32 non-volatile memory (NVS), survives reboots
- **Server Validation**: Server maintains last-seen nonce for each device
- **Rejection**: Any packet with nonce ≤ last-seen is rejected as a replay attack

**Implementation Details**:
```cpp
// ESP32 - Nonce generation
uint32_t nonce = load_nonce_from_nvs();
nonce++;
save_nonce_to_nvs(nonce);
```

```python
# Server - Nonce validation
def validate_nonce(device_id, nonce):
    last_nonce = get_last_nonce(device_id)
    if nonce <= last_nonce:
        raise ReplayAttackDetected()
    update_nonce(device_id, nonce)
```

#### Layer 2: Message Authentication (HMAC-SHA256)

**Problem**: Without authentication, an attacker could forge packets pretending to be a legitimate device.

**Solution**: Every packet is signed with **HMAC-SHA256**:

```
MAC = HMAC-SHA256(key, nonce + payload)
```

- **Pre-Shared Key**: 256-bit key embedded in ESP32 firmware and server
- **Signature Verification**: Server recalculates HMAC and compares
- **Tamper Detection**: Any modification to nonce or payload invalidates the MAC

**Security Strength**: 
- HMAC-SHA256 provides 256-bit security
- Computationally infeasible to forge valid signatures without the key
- Resistant to length-extension attacks

#### Layer 3: Confidentiality (Optional Encryption)

**Current Implementation**: Base64 encoding (mock encryption) for performance testing

**Production-Ready**: AES-128-CBC encryption support:

```cpp
// ESP32 - AES encryption
aes_encrypt(payload, key, iv, encrypted_payload);
base64_encode(encrypted_payload, base64_payload);
```

**Why AES-128-CBC**:
- **Performance**: Optimized for ESP32 hardware
- **Security**: 128-bit key provides strong security
- **Compatibility**: Widely supported and standardized

---

### FOTA (Firmware Over-The-Air) Security

The FOTA system implements **military-grade security** with multiple layers:

#### 1. AES-256-CBC Encryption

**Firmware is encrypted before transmission**:

```bash
# Server-side encryption
openssl enc -aes-256-cbc -in firmware.bin -out firmware.enc -K <key> -iv <iv>
```

- **Key Size**: 256-bit (32 bytes)
- **Block Size**: 128-bit (16 bytes)
- **Mode**: CBC (Cipher Block Chaining)
- **Chunk Size**: 2 KB for memory-efficient streaming

**Why Encrypt?**:
- Prevents firmware extraction by attackers
- Protects intellectual property
- Prevents analysis of firmware vulnerabilities

#### 2. SHA-256 Hash Verification

**After decryption, firmware integrity is verified**:

```cpp
// ESP32 - Hash verification
sha256_context ctx;
sha256_init(&ctx);
sha256_update(&ctx, firmware_data, firmware_size);
sha256_final(&ctx, calculated_hash);

if (memcmp(calculated_hash, manifest_hash, 32) != 0) {
    rollback_to_previous_firmware();
}
```

**Purpose**:
- Detect any corruption during download
- Verify decryption was successful
- Ensure firmware wasn't tampered with

#### 3. RSA-2048 Digital Signatures

**The firmware hash is signed with server's private key**:

```python
# Server - Sign firmware
signature = rsa_private_key.sign(
    firmware_hash,
    padding.PSS(mgf=padding.MGF1(hashes.SHA256())),
    hashes.SHA256()
)
```

**ESP32 verifies using embedded public key**:

```cpp
// ESP32 - Verify signature
bool valid = rsa_verify(
    manifest_hash,
    manifest_signature,
    public_key
);
```

**Why RSA-2048**:
- **Asymmetric**: Private key never leaves server
- **Key Size**: 2048-bit provides strong security until 2030+
- **Standard**: Widely used and tested
- **Embedded**: Public key (small) embedded in ESP32

#### 4. Automatic Rollback Protection

**Dual-partition system ensures device never becomes bricked**:

```
Flash Memory Layout:
┌────────────────┐
│   Bootloader   │
├────────────────┤
│  Partition 0   │ ← Previous firmware (fallback)
├────────────────┤
│  Partition 1   │ ← New firmware (current)
├────────────────┤
│   NVS / Data   │
└────────────────┘
```

**Rollback Triggers**:
1. **Hash Mismatch**: Firmware doesn't match manifest
2. **Signature Invalid**: RSA verification fails
3. **Boot Failure**: Watchdog timeout on first boot
4. **Manual**: Device reports failure after update

```cpp
// ESP32 - Rollback on failure
if (!verify_firmware()) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
}
```

---

## 📦 Compression System

### Intelligent Algorithm Selection

The system implements **4 specialized compression algorithms** and automatically selects the best for each data batch:

```
┌──────────────────────────────────────────────────────┐
│         Compression Tournament (Every 15s)           │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────┐
│  │Dictionary │  │  Temporal │  │ Semantic  │  │Bit │
│  │  (96.4%) │  │  Delta    │  │   RLE     │  │Pack│
│  │   5 bytes│  │  28 bytes │  │  42 bytes │  │70 B│
│  └───────────┘  └───────────┘  └───────────┘  └────┘
│                      ↓                               │
│              Select Best (Dictionary)               │
└──────────────────────────────────────────────────────┘
```

### Algorithm 1: Dictionary Compression (0xD0)

**Best For**: Stable, repetitive operational patterns

**Concept**: Build a dictionary of common sensor states and send only the index

**Example**:
```
State Dictionary:
0: [220V, 5A, 1100W, 50Hz, 25°C, ...]  ← "Normal daytime operation"
1: [0V, 0A, 0W, 0Hz, 18°C, ...]        ← "Night time / Off"
2: [230V, 10A, 2300W, 50Hz, 30°C, ...] ← "Peak load"
```

**Compressed Packet**:
```
┌────┐────┬────┐
│0xD0│ 0  │ 1  │  5 bytes total
└────┴────┴────┘
 Type Dict Dict
       ID   ID
```

**Compression Ratio**: 96.4% (140 bytes → 5 bytes)

**Implementation**:
```cpp
// ESP32 - Dictionary compression
uint8_t dict_id = find_matching_pattern(current_sample, dictionary);
if (dict_id != NOT_FOUND) {
    compressed[0] = 0xD0;  // Marker
    compressed[1] = dict_id;
    return 2;  // Only 2 bytes per sample!
}
```

**Dictionary Learning**:
- Automatically identifies frequently occurring patterns
- Updates dictionary with new patterns when space available
- Synchronized between ESP32 and server

### Algorithm 2: Temporal Delta Compression (0x71)

**Best For**: Slowly changing values (temperature, voltage)

**Concept**: Send only the *difference* from predicted value

**Prediction**: Linear extrapolation:
```
predicted = 2 × previous - previous_previous
```

**Example**:
```
Time:   T0      T1      T2      T3
Temp:   25.0°C  25.2°C  25.4°C  25.6°C

Prediction at T3: 2×25.4 - 25.2 = 25.6°C
Actual: 25.6°C
Delta: 0°C  ← Send only this!
```

**Variable-Length Encoding**:
```cpp
if (abs(delta) < 64)        // 7-bit
    encode_7bit(delta);
else if (abs(delta) < 128)  // 8-bit
    encode_8bit(delta);
else                        // 16-bit
    encode_16bit(delta);
```

**Compression Ratio**: ~80% (140 bytes → 28 bytes)

### Algorithm 3: Semantic RLE (0x50)

**Best For**: Flat-line data, device off-state

**Concept**: Group similar values by semantic meaning

**Example**:
```
Register Type | Values                    | Compressed
--------------|---------------------------|------------
Voltage       | 220, 220, 220, 220, 220  | 220 × 5
Current       | 0, 0, 0, 0, 0            | 0 × 5
Power         | 0, 0, 0, 0, 0            | 0 × 5
```

**Type-Aware Tolerances**:
- Voltage: ±2V considered identical
- Current: ±0.1A considered identical
- Temperature: ±0.5°C considered identical

**Compression Ratio**: ~70% (140 bytes → 42 bytes)

### Algorithm 4: Bit-Packing (0x01)

**Best For**: High-entropy data, fallback method

**Concept**: Pack values into minimum required bits

**Example**:
```
12-bit ADC reading: 0x0FA5 (4005 decimal)

Stored as 16-bit: 0x0FA5 = 0000111110100101
                            ^^^^
                            Wasted bits!

Bit-packed: 111110100101 (12 bits)
```

**Compression Ratio**: ~50% (140 bytes → 70 bytes)

---

## ⚡ Power Management

### Peripheral Gating Strategy

**Problem**: The Modbus UART transceiver consumes 10-20mA even when idle.

**Solution**: Power gate the UART, turning it on only when needed.

**Power States**:

```
┌─────────────────────────────────────────────────────┐
│                  Normal Operation                   │
│                                                     │
│  ┌──────┐    1.8s OFF     ┌──────┐   200ms ON     │
│  │ WiFi │ ──────────────→ │ UART │ ──────────→    │
│  │Active│                 │  ON  │                 │
│  └──────┘ ←────────────── └──────┘ ←──────────     │
│             Power Gating            Modbus Poll    │
└─────────────────────────────────────────────────────┘

Duty Cycle: 200ms / 2000ms = 10%
Power Savings: 10-20mA × 90% = 9-18mA continuous
```

**Implementation**:

```cpp
// ESP32 - Power gating control
#define UART_POWER_PIN GPIO_NUM_4

void power_on_uart() {
    gpio_set_level(UART_POWER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));  // Stabilization delay
}

void power_off_uart() {
    gpio_set_level(UART_POWER_PIN, 0);
}

// Main polling loop
while (1) {
    power_on_uart();
    modbus_read_registers();  // ~200ms
    power_off_uart();
    
    vTaskDelay(pdMS_TO_TICKS(1800));  // 1.8s sleep
}
```

**Statistics Tracking**:

```cpp
struct power_stats_t {
    uint32_t uart_on_time_ms;
    uint32_t uart_off_time_ms;
    uint32_t total_cycles;
    float duty_cycle_percent;
};
```

**Why Not Deep Sleep?**:
- **WiFi Connection**: Deep sleep disconnects WiFi
- **Reconnection Overhead**: 3-5 seconds to reconnect
- **Command Latency**: Real-time control requires low latency
- **Trade-off**: Peripheral gating provides significant savings while maintaining connectivity

---

## 🎯 Remote Control System

### Command Queue Architecture

**Bidirectional communication for real-time device control**:

```
User → Dashboard → Server → Queue → ESP32 → Modbus → Inverter
     ←          ←        ←       ←        ←        ←
          Status feedback
```

### Command Types

#### 1. Modbus Register Writes

**Direct control of inverter parameters**:

```python
# Server API
POST /api/command/modbus-write
{
    "device_id": "ESP32_001",
    "register": 40001,
    "value": 2000,  # Set power limit to 2000W
    "timeout": 10
}
```

**ESP32 Execution**:
```cpp
// ESP32 - Execute Modbus command
ModbusCommand cmd = poll_pending_command();
if (cmd.type == CMD_MODBUS_WRITE) {
    bool success = modbus_write_register(
        cmd.register_addr,
        cmd.value
    );
    report_status(success);
}
```

#### 2. Configuration Updates

**Remote parameter changes**:

```json
{
    "type": "config_update",
    "parameters": {
        "poll_interval": 5000,      // Change to 5 seconds
        "upload_interval": 30000,   // Change to 30 seconds
        "compression_enabled": true,
        "power_gating_enabled": true
    }
}
```

#### 3. Diagnostic Commands

**Remote debugging and maintenance**:

- `CMD_IMMEDIATE_UPLOAD`: Force immediate data upload
- `CMD_CLEAR_ERRORS`: Reset error counters
- `CMD_REBOOT`: Remote device reboot
- `CMD_DIAGNOSTICS`: Request diagnostic report

### Status Reporting

**Every command execution includes detailed feedback**:

```json
{
    "command_id": "cmd_12345",
    "status": "success",
    "execution_time_ms": 247,
    "modbus_response": "0x06",  // Modbus success code
    "timestamp": "2024-12-12T10:30:45Z"
}
```

**Error Reporting**:
```json
{
    "command_id": "cmd_12346",
    "status": "failed",
    "error_code": "MODBUS_TIMEOUT",
    "error_message": "Inverter did not respond within 5 seconds",
    "retry_count": 3
}
```

---

## 📊 Web Dashboard

See the main README for dashboard screenshots.

### Real-Time Features

#### Live Data Visualization

**Technology**: Recharts library with WebSocket updates

```javascript
// Frontend - Live chart
<LineChart data={liveData}>
  <Line dataKey="voltage" stroke="#8884d8" />
  <Line dataKey="current" stroke="#82ca9d" />
  <Line dataKey="power" stroke="#ffc658" />
</LineChart>
```

**Update Mechanism**:
1. ESP32 uploads data → Server
2. Server publishes to MQTT
3. Frontend WebSocket receives update
4. Chart automatically re-renders

**Update Frequency**: Real-time (as data arrives)

#### Compression Analytics

**Visual breakdown of compression performance**:

- **Pie Chart**: Distribution of compression methods used
- **Bar Chart**: Compression ratios over time
- **Metrics**: Total bandwidth saved, average compression ratio

#### Security Monitoring

**Real-time security metrics**:

- Current nonce value
- HMAC validation success rate
- Replay attack detection count
- Last successful authentication timestamp

---

## 🔌 Modbus Communication

### Ring Buffer Architecture

**Problem**: Data acquisition (2s) and upload (15s) happen at different rates.

**Solution**: 7-sample circular buffer

```
┌───────────────────────────────────────┐
│     Ring Buffer (7 samples)           │
├───────────────────────────────────────┤
│ [0] [1] [2] [3] [4] [5] [6]          │
│  ↑                       ↑            │
│ Write                  Read           │
│ Ptr                    Ptr            │
└───────────────────────────────────────┘

Every 2s:  Write new sample, advance write pointer
Every 15s: Read all 7 samples, compress, upload
```

**Benefits**:
- **No Data Loss**: Buffer survives temporary network outages
- **Batch Compression**: 7 samples compressed together for better ratios
- **Decoupling**: Acquisition and upload operate independently

**Implementation**:
```cpp
struct RingBuffer {
    ModbusSample samples[7];
    uint8_t write_index;
    uint8_t read_index;
    uint8_t count;
};

void add_sample(ModbusSample sample) {
    buffer.samples[buffer.write_index] = sample;
    buffer.write_index = (buffer.write_index + 1) % 7;
    buffer.count = min(buffer.count + 1, 7);
}

void read_all_samples(ModbusSample* output) {
    memcpy(output, buffer.samples, sizeof(buffer.samples));
}
```

---

## Summary

The EcoWatt system combines multiple advanced features:

- **Security**: 3-layer upload protection + RSA-signed FOTA
- **Efficiency**: 96.4% compression with intelligent algorithm selection
- **Reliability**: Automatic rollback, error recovery, ring buffering
- **Control**: Real-time bidirectional communication
- **Power**: 10-20mA savings through peripheral gating
- **Visibility**: Professional dashboard with live monitoring

[← Back to Main README](../README.md)
