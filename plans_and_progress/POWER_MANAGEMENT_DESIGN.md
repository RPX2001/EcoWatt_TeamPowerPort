# Power Management Implementation Design
**Date:** 2025-10-30  
**Milestone:** 5  
**Status:** In Progress - Task 4/14 Complete

## 🚀 Implementation Progress

### ✅ Completed Tasks
- **Task 1-2:** Design & Analysis Complete (2025-10-30)
- **Task 3:** NVS Storage Implementation (2025-10-30)
  - Added 6 new functions to `nvs.h/cpp`
  - Power namespace with defaults: enabled=false, strategy=1, poll=5min
- **Task 4:** Power Strategy Modes (2025-10-30) ⬅️ LATEST
  - Added `PowerStrategy` enum to `power_management.h`
  - Implemented `enable()`, `setStrategy()`, `getStrategy()`, `isEnabled()`
  - Init() now loads settings from NVS
  - Strategy-specific behaviors defined (sleep aggressiveness, peripheral gating)

### 🔄 In Progress
- **Task 5:** Self-Energy Reporting (Next)

### 📋 Pending Tasks
- Tasks 5-14: See detailed checklist below

---

## 📋 Executive Summary

This document details the complete architecture for adding Power Management to the EcoWatt system. The implementation is **modular, non-breaking, and easily removable** if needed.

### Current State Analysis
✅ **ESP32**: Power management code exists (`power_management.cpp/h`, `peripheral_power.cpp/h`)  
✅ **Flask**: Infrastructure ready (device registry, config system, time-series data)  
✅ **Frontend**: UI framework in place (Configuration page, Dashboard page)  

### Gaps Identified
❌ **ESP32**: No remote config, no strategy modes, no energy reporting  
❌ **Flask**: No power endpoints, no energy data storage  
❌ **Frontend**: No power UI components

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         FRONTEND (React)                        │
│  ┌────────────────────┐        ┌─────────────────────────────┐ │
│  │ Configuration Tab  │        │      Dashboard Page          │ │
│  │ - Power On/Off     │        │  - Current Power Gauge       │ │
│  │ - Strategy Select  │        │  - Energy Saved Widget       │ │
│  │ - Poll Interval    │        │  - 24h Power Timeline Chart  │ │
│  └────────┬───────────┘        └─────────────┬───────────────┘ │
└───────────┼──────────────────────────────────┼─────────────────┘
            │                                  │
            │ POST /power/<device_id>          │ GET /power/energy/<device_id>
            │                                  │
┌───────────▼──────────────────────────────────▼─────────────────┐
│                      FLASK SERVER (Python)                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Power Routes (routes/power_routes.py)                    │  │
│  │ - GET/POST /power/<device_id>           (config)         │  │
│  │ - POST /power/energy/<device_id>        (report)         │  │
│  │ - GET /power/energy/<device_id>         (history)        │  │
│  └────────┬─────────────────────────────────────────────────┘  │
│           │                                                     │
│  ┌────────▼──────────────────────────────────┐                 │
│  │ Device Registry (device_configs dict)     │                 │
│  │ + power_enabled: bool                     │                 │
│  │ + power_strategy: str                     │                 │
│  │ + energy_poll_interval: int               │                 │
│  └───────────────────────────────────────────┘                 │
│  ┌───────────────────────────────────────────┐                 │
│  │ Energy Database (SQLite)                  │                 │
│  │ - device_id, timestamp, avg_current_ma,   │                 │
│  │   energy_saved_mah, uptime_ms, strategy   │                 │
│  └───────────────────────────────────────────┘                 │
└───────────────────────┬─────────────────────────────────────────┘
                        │
                        │ GET /config/<device_id>
                        │ POST /power/energy/<device_id>
                        │
┌───────────────────────▼─────────────────────────────────────────┐
│                      ESP32 (FreeRTOS)                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ ConfigManager (config_manager.cpp)                       │  │
│  │ - Parse power config from pending_config                 │  │
│  │ - Apply to PowerManagement class                         │  │
│  │ - Send acknowledgment                                    │  │
│  └────────┬─────────────────────────────────────────────────┘  │
│           │                                                     │
│  ┌────────▼──────────────────────────────────┐                 │
│  │ PowerManagement (power_management.cpp)    │                 │
│  │ + Strategy modes (3 levels)               │                 │
│  │ + Energy statistics collection            │                 │
│  │ + Enable/disable control                  │                 │
│  └────────┬──────────────────────────────────┘                 │
│           │                                                     │
│  ┌────────▼──────────────────────────────────┐                 │
│  │ NVS Storage (nvs.cpp)                     │                 │
│  │ + power_enabled: bool                     │                 │
│  │ + power_strategy: uint8_t (0-2)           │                 │
│  │ + energy_poll_freq: uint64_t              │                 │
│  └───────────────────────────────────────────┘                 │
│  ┌───────────────────────────────────────────┐                 │
│  │ Energy Report Task (task_manager.cpp)     │                 │
│  │ - Periodic collection of PowerStats       │                 │
│  │ - JSON formatting                         │                 │
│  │ - HTTP POST to Flask                      │                 │
│  └───────────────────────────────────────────┘                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📊 Data Schema Definitions

### 1. ESP32 NVS Schema
```cpp
// Stored in NVS with key prefixes
"power_enabled"         : bool    // Default: false
"power_strategy"        : uint8_t // 0=Aggressive, 1=Balanced, 2=Conservative
"energy_poll_interval"  : uint64_t // Microseconds, default: 300000000 (5 min)
```

### 2. Flask Device Config Schema
```json
{
  "device_id": "ESP32_001",
  "sampling_interval": 10,
  "upload_interval": 900,
  "config_poll_interval": 60,
  "command_poll_interval": 30,
  "firmware_check_interval": 3600,
  "compression_enabled": true,
  "available_registers": [...],
  "selected_registers": [...],
  
  // NEW: Power Management Config
  "power_management": {
    "enabled": false,
    "strategy": "balanced",  // "aggressive" | "balanced" | "conservative"
    "energy_poll_interval": 300  // seconds (5 minutes)
  }
}
```

### 3. Energy Report Payload (ESP32 → Flask)
```json
{
  "device_id": "ESP32_001",
  "timestamp": 1698700000,
  "power_stats": {
    "avg_current_ma": 142.5,
    "energy_saved_mah": 23.4,
    "uptime_ms": 3600000,
    "strategy": "balanced",
    "enabled": true,
    "cpu_frequency_mhz": 240,
    "high_perf_time_pct": 45.2,
    "normal_time_pct": 30.1,
    "low_power_time_pct": 20.5,
    "sleep_time_pct": 4.2
  }
}
```

### 4. Energy History Database Schema (SQLite)
```sql
CREATE TABLE IF NOT EXISTS power_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    timestamp DATETIME NOT NULL,
    avg_current_ma REAL NOT NULL,
    energy_saved_mah REAL NOT NULL,
    uptime_ms INTEGER NOT NULL,
    strategy TEXT NOT NULL,
    enabled BOOLEAN NOT NULL,
    cpu_freq_mhz INTEGER,
    high_perf_pct REAL,
    normal_pct REAL,
    low_power_pct REAL,
    sleep_pct REAL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_power_device_time ON power_reports(device_id, timestamp);
```

---

## 🔧 Power Strategy Definitions

### Strategy 1: AGGRESSIVE (Maximum Power Savings)
**Goal:** Minimize power consumption at cost of some performance  
**Use Case:** Battery-powered deployments, low-traffic scenarios

```cpp
CPU Frequency:
  - Active operations: 240 MHz (WiFi requirement)
  - Idle/waiting: 160 MHz (WiFi-safe minimum)
  - Between tasks: Light sleep 10-50ms

Peripheral Gating:
  - UART: Disable immediately after Modbus poll
  - WiFi: Modem sleep between uploads (if possible)
  
Task Timing:
  - Maximize sleep between polls
  - Bundle operations to reduce wake cycles
  
Estimated Savings: 15-25% vs always-on 240MHz
```

### Strategy 2: BALANCED (Default)
**Goal:** Balance power and performance  
**Use Case:** Standard deployments

```cpp
CPU Frequency:
  - Active operations: 240 MHz
  - Idle: 160 MHz
  - Light sleep: 5-20ms between tasks

Peripheral Gating:
  - UART: Disable when not polling
  - WiFi: Always active
  
Task Timing:
  - Normal scheduling
  
Estimated Savings: 8-15% vs always-on 240MHz
```

### Strategy 3: CONSERVATIVE (Performance Priority)
**Goal:** Maintain responsiveness, minimal power optimization  
**Use Case:** Mission-critical deployments

```cpp
CPU Frequency:
  - Always 240 MHz
  
Peripheral Gating:
  - UART: Disable when idle (only change)
  - WiFi: Always active
  
Task Timing:
  - No additional delays
  
Estimated Savings: 3-5% vs always-on 240MHz
```

---

## 🔌 API Endpoint Specifications

### 1. GET /power/<device_id>
**Purpose:** Retrieve current power configuration  
**Response:**
```json
{
  "device_id": "ESP32_001",
  "power_management": {
    "enabled": true,
    "strategy": "balanced",
    "energy_poll_interval": 300
  }
}
```

### 2. POST /power/<device_id>
**Purpose:** Update power configuration  
**Request:**
```json
{
  "enabled": true,
  "strategy": "aggressive",
  "energy_poll_interval": 600
}
```
**Response:**
```json
{
  "success": true,
  "message": "Power configuration updated",
  "pending_config_id": "cfg_12345"
}
```

### 3. POST /power/energy/<device_id>
**Purpose:** Accept energy report from ESP32  
**Request:** See "Energy Report Payload" above  
**Response:**
```json
{
  "success": true,
  "message": "Energy report stored"
}
```

### 4. GET /power/energy/<device_id>?period=24h
**Purpose:** Retrieve energy history  
**Query Params:**
- `period`: "1h" | "24h" | "7d" | "30d"
- `limit`: max records (default 100)

**Response:**
```json
{
  "device_id": "ESP32_001",
  "period": "24h",
  "reports": [
    {
      "timestamp": "2025-10-30T10:00:00Z",
      "avg_current_ma": 142.5,
      "energy_saved_mah": 23.4,
      "strategy": "balanced"
    }
  ],
  "summary": {
    "avg_current_ma": 145.2,
    "total_energy_saved_mah": 156.7,
    "uptime_hours": 24.0
  }
}
```

---

## 🔄 Configuration Flow

### Scenario: User Enables Power Management from Frontend

```
1. Frontend: User toggles power ON, selects "Aggressive" strategy
   ↓
2. Frontend → Flask: POST /power/ESP32_001
   {
     "enabled": true,
     "strategy": "aggressive",
     "energy_poll_interval": 600
   }
   ↓
3. Flask: Updates device_configs[ESP32_001]["power_management"]
   Flask: Creates pending_config with new power settings
   ↓
4. ESP32 → Flask: GET /config/ESP32_001 (periodic check)
   ↓
5. Flask → ESP32: Returns config with is_pending=true
   {
     "is_pending": true,
     "pending_config": {
       "config_update": {
         "power_management": {
           "enabled": true,
           "strategy": "aggressive",
           "energy_poll_interval": 600
         }
       }
     }
   }
   ↓
6. ESP32: ConfigManager parses power_management block
   ESP32: Calls PowerManagement::setStrategy(AGGRESSIVE)
   ESP32: Calls PowerManagement::enable(true)
   ESP32: Updates NVS
   ESP32: Sends acknowledgment
   ↓
7. ESP32 → Flask: POST /config/ESP32_001/ack
   ↓
8. Flask: Clears pending_config
   ↓
9. ESP32: Starts energy reporting task (every 600 seconds)
   ↓
10. ESP32 → Flask: POST /power/energy/ESP32_001 (periodic)
   ↓
11. Frontend → Flask: GET /power/energy/ESP32_001 (periodic poll)
   ↓
12. Frontend: Updates dashboard with energy stats
```

---

## 🧩 Modularity & Removability

### Compile-Time Flag
```ini
# platformio.ini
[env:esp32dev]
build_flags = 
    -DENABLE_POWER_MANAGEMENT  # Comment out to disable
```

### Conditional Compilation Pattern
```cpp
#ifdef ENABLE_POWER_MANAGEMENT
    PowerManagement::init();
    // ... power management code
#else
    // Simplified: Always run at 240MHz, no power stats
#endif
```

### Task Conditional Creation
```cpp
bool TaskManager::init(...) {
    // Always created
    createTask(sensorPollTask, ...);
    createTask(uploadTask, ...);
    
    #ifdef ENABLE_POWER_MANAGEMENT
    createTask(energyReportTask, ...);
    #endif
    
    return true;
}
```

### Zero Impact When Disabled
- No power-related network traffic
- No additional NVS storage
- No CPU overhead
- System functions normally at 240MHz

---

## ✅ Implementation Checklist

### Phase 1: ESP32 Foundation (Tasks 3-5)
- [ ] Add NVS keys: `power_enabled`, `power_strategy`, `energy_poll_interval`
- [ ] Add getters/setters to `nvs.h/cpp`
- [ ] Define `PowerStrategy` enum in `power_management.h`
- [ ] Implement 3 strategy modes in `PowerManagement` class
- [ ] Add `setStrategy()`, `getStrategy()`, `enable()` methods
- [ ] Create `energyReportTask` in `task_manager.cpp`
- [ ] Format energy JSON payload
- [ ] POST to `/power/energy/<device_id>`

### Phase 2: Flask Backend (Tasks 6-7)
- [ ] Create `routes/power_routes.py`
- [ ] Implement GET `/power/<device_id>`
- [ ] Implement POST `/power/<device_id>`
- [ ] Implement POST `/power/energy/<device_id>`
- [ ] Implement GET `/power/energy/<device_id>`
- [ ] Add `power_reports` table to `database.py`
- [ ] Add power storage/retrieval methods
- [ ] Update device config schema

### Phase 3: Frontend (Tasks 8-9)
- [ ] Add power section to `Configuration.jsx`
- [ ] Create power toggle component
- [ ] Create strategy dropdown
- [ ] Create energy poll interval input
- [ ] Add energy widgets to `Dashboard.jsx`
- [ ] Create power consumption gauge
- [ ] Create energy saved display
- [ ] Create 24h timeline chart (Chart.js)
- [ ] Add API calls in `src/api/`

### Phase 4: Integration (Tasks 10-12)
- [ ] Extend `ConfigManager` to parse `power_management` block
- [ ] Apply power settings on config change
- [ ] Add compile flag to `platformio.ini`
- [ ] Wrap code in `#ifdef ENABLE_POWER_MANAGEMENT`
- [ ] Test with flag ON and OFF
- [ ] Implement power acknowledgment flow

### Phase 5: Testing & Documentation (Tasks 13-14)
- [ ] Test enable/disable from frontend
- [ ] Test strategy changes
- [ ] Verify energy reports arrive
- [ ] Check historical data display
- [ ] Test system with power OFF
- [ ] Verify no impact on data acquisition
- [ ] Update `API_SYNC_MAPPING.md`
- [ ] Create power management user guide

---

## 🎯 Success Criteria

✅ **Functional Requirements:**
1. User can enable/disable power management from frontend
2. User can select strategy (aggressive/balanced/conservative)
3. ESP32 applies strategy and reports energy stats
4. Frontend displays real-time and historical power data
5. System works normally with power management disabled

✅ **Non-Functional Requirements:**
1. No impact on existing data acquisition
2. No breaking changes to existing APIs
3. Modular code that can be easily removed
4. Backward compatible NVS storage
5. Clean separation of concerns

✅ **Performance Requirements:**
1. Energy reporting overhead < 1% CPU time
2. Power strategy change applies within 1 second
3. Historical data query < 500ms for 24h period

---

## 📝 Notes & Constraints

### ESP32 Constraints
- WiFi requires minimum 160 MHz CPU frequency
- Light sleep may interfere with hardware timers
- NVS writes are slow (~100ms), use sparingly
- FreeRTOS tasks have stack size limits

### Flask Constraints
- Time-series data can grow large, implement retention
- Device config stored in RAM, should persist to DB
- SQLite may need WAL mode for concurrent writes

### Frontend Constraints
- Chart.js bundle size (~200KB)
- Real-time updates need efficient polling
- Mobile-responsive design required

---

**END OF DESIGN DOCUMENT**
