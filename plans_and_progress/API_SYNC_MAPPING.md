# API Synchronization Mapping
**Date:** 2025-10-28  
**Purpose:** Complete mapping of Flask ↔ ESP32 ↔ Frontend endpoints

---

## 📡 **ESP32 → Flask (Requests from ESP32)**

| ESP32 Function | Endpoint | Method | Flask Route | Status |
|----------------|----------|--------|-------------|--------|
| Data Upload | `/aggregated/ESP32_001` | POST | ✅ `aggregation_routes.py:26` | **Working** |
| Command Poll | `/commands/ESP32_001/poll` | GET | ✅ `command_routes.py:132` | **Working** |
| Command Result | `/commands/ESP32_001/result` | POST | ✅ `command_routes.py:174` | **Working** |
| Config Check | `/config/ESP32_001` | GET | ✅ `config_routes.py:127` | **Working** |
| OTA Check | `/ota/check/ESP32_EcoWatt_Smart?version=1.0.4` | GET | ✅ `ota_routes.py:28` | **Working** |
| OTA Status (report completion) | ❌ **MISSING** | POST | ❌ **NOT IMPLEMENTED** | **MISSING** |
| Config Ack (confirm applied) | ❌ **MISSING** | POST | ❌ **NOT IMPLEMENTED** | **MISSING** |

---

## 🌐 **Frontend → Flask (Requests from UI)**

### **Devices**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `devices.js` | Get all devices | `/devices` | ✅ `device_routes.py:48` | ✅ Working |
| `devices.js` | Get device by ID | `/devices/{id}` | ✅ `device_routes.py:79` | ✅ Working |
| `devices.js` | Create device | `/devices` | ✅ `device_routes.py:113` | ✅ Working |
| `devices.js` | Update device | `/devices/{id}` | ✅ `device_routes.py:184` | ✅ Working |
| `devices.js` | Delete device | `/devices/{id}` | ✅ `device_routes.py:242` | ✅ Working |

### **Data Aggregation**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `aggregation.js` | Get latest data | `/aggregation/latest/{id}` OR `/latest/{id}` | ✅ `aggregation_routes.py:382-383` | ✅ Working |
| `aggregation.js` | Get historical data | `/aggregation/historical/{id}` | ✅ `aggregation_routes.py:327` | ⚠️ **IN-MEMORY ONLY** |
| `aggregation.js` | Export CSV | `/export/{id}/csv` | ✅ `aggregation_routes.py:423` | ⚠️ **IN-MEMORY ONLY** |
| `aggregation.js` | Validate compression | `/compression/validate` | ✅ `aggregation_routes.py:253` | ✅ Working |
| `aggregation.js` | Get compression stats | `/compression/stats` | ✅ `aggregation_routes.py:289` | ✅ Working |

**ISSUE:** Historical data only stored in RAM (`data_handler.py` uses dict). **NEEDS PERSISTENT STORAGE**.

### **Commands**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `commands.js` | Send command | `/commands/{device_id}` | ✅ `command_routes.py:53` | ✅ Working |
| `commands.js` | Get command status | `/commands/status/{command_id}` | ✅ `command_routes.py:245` | ✅ Working |
| `commands.js` | Get command history | `/commands/{device_id}/history` | ✅ `command_routes.py:282` | ⚠️ **IN-MEMORY ONLY** |
| `commands.js` | Get command stats | `/commands/stats` | ✅ `command_routes.py:320` | ✅ Working |

**ISSUE:** Command history stored in RAM. **NEEDS PERSISTENT STORAGE**.

### **Configuration**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `config.js` | Get config | `/config/{device_id}` | ✅ `config_routes.py:127` | ✅ Working |
| `config.js` | Update config | `/config/{device_id}` | ✅ `config_routes.py:161` | ✅ Working |
| `config.js` | Get config history | `/config/{device_id}/history` | ✅ `config_routes.py:262` | ⚠️ **IN-MEMORY ONLY** |
| `config.js` | Get config ack status | ❌ **MISSING** | ❌ **NOT IMPLEMENTED** | **MISSING** |

**ISSUE:** Config should REPLACE not QUEUE. ESP32 should send acknowledgment.

### **OTA Updates**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `ota.js` | Check for update | `/ota/check/{device_id}` | ✅ `ota_routes.py:28` | ✅ Working |
| `ota.js` | Initiate OTA | `/ota/initiate/{device_id}` | ✅ `ota_routes.py:51` | ✅ Working |
| `ota.js` | Get OTA status | `/ota/status/{device_id}` | ✅ `ota_routes.py:198` | ✅ Working |
| `ota.js` | Get all OTA status | `/ota/status` | ✅ `ota_routes.py:218` | ✅ Working |
| `ota.js` | Get OTA stats | `/ota/stats` | ✅ `ota_routes.py:237` | ✅ Working |
| `ota.js` | Upload firmware | `/ota/upload` | ✅ `ota_routes.py:340` | ✅ Working |
| `ota.js` | List firmwares | `/ota/firmwares` | ✅ `ota_routes.py:435` | ✅ Working |
| `ota.js` | Receive ESP32 completion status | `/ota/<device_id>/complete` | ✅ `ota_routes.py:479` | ⚠️ **ESP32 NOT SENDING** |

**ISSUE:** ESP32 doesn't report OTA completion (success/fail/version) back to Flask.

### **Diagnostics**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `diagnostics.js` | Get all diagnostics | `/diagnostics` | ✅ `diagnostics_routes.py:23` | ✅ Working |
| `diagnostics.js` | Get device diagnostics | `/diagnostics/{device_id}` | ✅ `diagnostics_routes.py:43` | ✅ Working |
| `diagnostics.js` | Upload diagnostics | `/diagnostics/{device_id}` | ✅ `diagnostics_routes.py:64` | ✅ Working |

### **Faults**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `faults.js` | Inject fault | `/fault/inject` | ✅ `fault_routes.py:34` | ✅ Working |
| `faults.js` | Get fault status | `/fault/status` | ✅ `fault_routes.py:252` | ✅ Working |
| `faults.js` | Clear fault | `/fault/clear` | ✅ `fault_routes.py:287` | ✅ Working |
| `faults.js` | Get fault types | `/fault/types` | ✅ `fault_routes.py:329` | ✅ Working |

### **Security**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `security.js` | Validate payload | `/security/validate/{device_id}` | ✅ `security_routes.py:23` | ✅ Working |
| `security.js` | Get security stats | `/security/stats` | ✅ `security_routes.py:64` | ✅ Working |
| `security.js` | Reset device nonce | `/security/nonces/{device_id}` | ✅ `security_routes.py:108` | ✅ Working |

### **Utilities**
| Frontend File | Function | Endpoint | Flask Route | Status |
|---------------|----------|----------|-------------|--------|
| `utilities.js` | Prepare firmware | `/utilities/firmware/prepare` | ✅ `utilities_routes.py:24` | ✅ Working |
| `utilities.js` | Generate keys | `/utilities/keys/generate` | ✅ `utilities_routes.py:116` | ✅ Working |
| `utilities.js` | Benchmark compression | `/utilities/compression/benchmark` | ✅ `utilities_routes.py:186` | ✅ Working |
| `utilities.js` | Get system info | `/utilities/info` | ✅ `utilities_routes.py:256` | ✅ Working |

---

## ✅ **VERIFIED STATUS - All Systems Synchronized**

### **Configuration Management** ✅
- **Behavior**: Latest config **REPLACES** pending (not queue)
- **Flask**: `Database.save_config()` deletes pending before insert
- **ESP32**: Polls `/config/<device_id>`, applies, sends ACK to `/config/<device_id>/acknowledge`
- **Frontend**: ConfigHistory component properly connected, shows real data

### **Command Management** ✅  
- **Behavior**: Commands **QUEUE** (all sent to ESP32)
- **Flask**: `Database.save_command()` uses INSERT (appends to queue)
- **ESP32**: Polls `/commands/<device_id>/poll`, executes, sends result to `/commands/<device_id>/result`
- **Frontend**: CommandHistory and CommandQueue properly connected, show real data

### **Firmware Management** ✅
- **Flask**: Lists firmwares from `/firmware` folder via `/ota/firmwares`
- **Frontend**: FirmwareList component connected (fixed: `firmwares` not `firmware`)
- **ESP32**: Reports OTA completion via `reportOTACompletionStatus()` to `/ota/<device_id>/complete`

### **Data Storage** ✅
- **Flask**: All decompressed data saved with timestamps via `Database.save_sensor_data()`
- **Endpoint**: `/aggregation/historical/<device_id>` returns historical data
- **Frontend**: Dashboard shows historical data in Charts and Table views

---

## 🔴 **CRITICAL MISSING FEATURES**

### 1. **Persistent Data Storage** ⚠️ URGENT
- **Issue:** All sensor data stored in RAM (lost on restart)
- **Impact:** Cannot show historical data, cannot analyze trends
- **Solution:** Implement SQLite/JSON/CSV storage

### 2. **Config Queue Logic** ⚠️ IMPORTANT
- **Issue:** Config changes queue like commands (should replace)
- **Impact:** ESP32 applies old configs even if newer one pending
- **Solution:** Flask keeps only LATEST pending config per device

### 3. **ESP32 Acknowledgments** ⚠️ IMPORTANT
- **Issue:** ESP32 never confirms config/command/OTA execution
- **Impact:** Frontend shows "pending" forever, no error feedback
- **Solution:** 
  - Add `/config/{device_id}/acknowledge` endpoint (ESP32 → Flask)
  - Add `/commands/{device_id}/acknowledge` endpoint (ESP32 → Flask)
  - Add `/ota/{device_id}/complete` endpoint (ESP32 → Flask)

### 4. **Frontend Missing Features** ⚠️ MODERATE
- **Issue:** Frontend doesn't show:
  - Historical sensor data (only latest)
  - Command execution history properly
  - Config acknowledgment status
  - OTA completion status
  - Available firmware versions list
- **Solution:** Update frontend components

### 5. **Command History** ⚠️ MODERATE
- **Issue:** Command history in memory only
- **Impact:** Lost on server restart
- **Solution:** Persist to database

---

## 📊 **Data Flow Architecture**

```
ESP32 (Every 15s)
    │
    ├─ POST /aggregated/ESP32_001 ──────> Flask saves to RAM ──> Frontend polls /latest/ESP32_001
    │                                          │
    │                                          └─> ❌ MISSING: Save to persistent storage
    │
    ├─ GET /commands/ESP32_001/poll ────> Flask returns queued commands
    │                                          │
    │                                          └─> ESP32 executes
    │                                               │
    │                                               └─> ❌ MISSING: POST /commands/.../acknowledge
    │
    ├─ GET /config/ESP32_001 ───────────> Flask returns latest config
    │                                          │
    │                                          └─> ESP32 applies
    │                                               │
    │                                               └─> ❌ MISSING: POST /config/.../acknowledge
    │
    └─ GET /ota/check/... ──────────────> Flask returns update info
                                               │
                                               └─> ESP32 downloads & installs
                                                    │
                                                    └─> ❌ MISSING: POST /ota/.../complete

Frontend (User Actions)
    │
    ├─ POST /commands/{id} ─────> Flask queues command ──> ESP32 polls
    ├─ PUT /config/{id} ────────> Flask stores config ───> ESP32 polls
    └─ POST /ota/initiate/{id} ─> Flask prepares OTA ────> ESP32 downloads
```

---

## ✅ **Implementation Priorities**

1. **Phase 1: Persistent Data Storage** (CRITICAL)
   - Implement SQLite database for sensor data
   - Migrate from in-memory dict to DB
   - Add timestamp indexing

2. **Phase 2: ESP32 Acknowledgments** (CRITICAL)
   - Add acknowledgment endpoints to Flask
   - Implement ESP32 POST requests after execution
   - Update Frontend to show status

3. **Phase 3: Config Queue Fix** (IMPORTANT)
   - Change config handler to replace not queue
   - Update frontend config UI

4. **Phase 4: Frontend Enhancements** (IMPORTANT)
   - Historical data charts
   - Command history table
   - Firmware version list
   - Status indicators

5. **Phase 5: Code Cleanup** (MODERATE)
   - Remove unused endpoints
   - Delete dead code
   - Verify FreeRTOS stability

