# 📂 Complete File Structure - After Improvements

**EcoWatt Project - Team PowerPort**  
**Shows all new files to be created**

---

## 🗂️ Root Directory

```
EcoWatt_TeamPowerPort/
│
├── 📄 IMPROVEMENT_PLAN.md           ← Main technical document (UPDATED)
├── 📄 TODO_CHECKLIST.md             ← Action items checklist (UPDATED)
├── 📄 ARCHITECTURE_IMPROVEMENTS.md  ← System diagrams (NEW)
├── 📄 IMPROVEMENT_SUMMARY.md        ← Executive overview (NEW)
├── 📄 UPDATE_SUMMARY.md             ← Latest additions summary (NEW)
├── 📄 FILE_STRUCTURE.md             ← This file (NEW)
│
├── README.md
├── PHASE_2.md
├── PHASE_3.md
├── PHASE_4.md
│
├── docs/
│   └── [PDF documentation files]
│
├── PIO/ECOWATT/                     ← ESP32 Project Directory
│   └── [See ESP32 Structure below]
│
└── flask/                           ← Flask Server Directory
    └── [See Flask Structure below]
```

---

## 🔌 ESP32 Project Structure

```
PIO/ECOWATT/
│
├── 📄 justfile                        ✨ NEW - Build automation (40+ recipes)
├── 📄 wokwi.toml                     ✨ NEW - Wokwi simulator config
├── 📄 diagram.json                   ✨ NEW - Wokwi circuit diagram
├── 📄 platformio.ini                 🔧 MODIFY - Add [env:wokwi]
├── 📄 Doxyfile                       ✨ NEW (optional) - Documentation
│
├── include/
│   ├── test_config.h                 ✨ NEW - Test configuration
│   └── [existing headers]
│
├── src/
│   ├── main.cpp                      🔧 MODIFY - Add diagnostics endpoint
│   │
│   ├── application/
│   │   ├── diagnostics.h             ✨ NEW - Event logging interface
│   │   ├── diagnostics.cpp           ✨ NEW - Event logging implementation
│   │   ├── aggregation.h             ✨ NEW - Data aggregation interface
│   │   ├── aggregation.cpp           ✨ NEW - Data aggregation implementation
│   │   ├── compression.cpp           🔧 MODIFY - Fix statistics calculation
│   │   ├── compression.h             🔧 MODIFY - Add aggregation support
│   │   ├── OTAManager.cpp            🔧 MODIFY - Enhanced diagnostics
│   │   └── [other existing files]
│   │
│   ├── driver/
│   │   ├── protocol_adapter.h        🔧 MODIFY - Add error enums
│   │   ├── protocol_adapter.cpp      🔧 MODIFY - Add CRC validation
│   │   ├── wokwi_mock.h              ✨ NEW - Wokwi simulator mocks
│   │   ├── wokwi_mock.cpp            ✨ NEW - Mock HTTP/WiFi responses
│   │   └── [other existing files]
│   │
│   └── peripheral/
│       ├── arduino_wifi.cpp          🔧 MODIFY - Add Wokwi support
│       └── [other existing files]
│
├── test/                             ✨ NEW DIRECTORY
│   ├── test_diagnostics_unit.cpp     ✨ NEW - Diagnostics unit tests
│   ├── test_protocol_diagnostics.cpp ✨ NEW - Protocol integration tests
│   ├── test_aggregation_unit.cpp     ✨ NEW - Aggregation unit tests
│   ├── test_compression_methods.cpp  ✨ NEW - Compression unit tests
│   ├── test_security_unit.cpp        ✨ NEW - Security unit tests
│   └── README.md                     ✨ NEW - Test documentation
│
└── lib/
    └── [existing libraries]
```

---

## 🌐 Flask Server Structure

```
flask/
│
├── 📄 justfile                        ✨ NEW - Server automation (50+ recipes)
├── 📄 requirements.txt                🔧 MODIFY - Add pytest, black, etc.
├── 📄 config.py                       🔧 MODIFY - Add test settings
│
├── flask_server_hivemq.py            🔧 MODIFY - Add fault injection endpoints
├── server_security_layer.py          🔧 MODIFY - Add persistent nonce storage
├── firmware_manager.py               🔧 MODIFY - Add FOTA fault injection
├── command_manager.py                ← Existing
│
├── fault_injector.py                 ✨ NEW - Fault injection module
├── security_tester.py                ✨ NEW - Security attack simulation
├── benchmark_compression.py          ✨ NEW - Python compression validator
│
├── nonce_state.json                  ✨ NEW - Persistent nonce storage (runtime)
├── security_audit.log                ✨ NEW - Security events log (runtime)
│
├── tests/                            ✨ NEW DIRECTORY
│   ├── __init__.py                   ✨ NEW
│   ├── test_protocol.py              ✨ NEW - Protocol adapter tests
│   ├── test_compression.py           ✨ NEW - Compression tests
│   ├── test_compression_crossval.py  ✨ NEW - ESP32 ↔ Python validation
│   ├── test_security.py              ✨ NEW - Security layer tests
│   ├── test_security_replay.py       ✨ NEW - Replay attack test
│   ├── test_security_tamper.py       ✨ NEW - Tamper detection test
│   ├── test_fota.py                  ✨ NEW - FOTA tests
│   ├── test_integration.py           ✨ NEW - End-to-end integration tests
│   └── README.md                     ✨ NEW - Test documentation
│
├── firmware/
│   ├── firmware_1.0.4_encrypted.bin  ← Existing
│   ├── firmware_1.0.4_manifest.json  ← Existing
│   └── test_firmware_bad_hash.bin    ✨ NEW - Test firmware (for FOTA tests)
│
└── keys/
    └── [existing key files]
```

---

## 📊 File Statistics

### New Files to Create

#### ESP32 Side
- **Source Code**: 10 files
  - diagnostics.{h,cpp}
  - aggregation.{h,cpp}
  - wokwi_mock.{h,cpp}
  - test_config.h
  - Modified: 3 files (main.cpp, protocol_adapter.cpp, compression.cpp)

- **Tests**: 5 files
  - test_diagnostics_unit.cpp
  - test_protocol_diagnostics.cpp
  - test_aggregation_unit.cpp
  - test_compression_methods.cpp
  - test_security_unit.cpp

- **Configuration**: 3 files
  - justfile
  - wokwi.toml
  - diagram.json

**Total ESP32**: 18 new files + 3 modified

---

#### Flask Side
- **Source Code**: 3 files
  - fault_injector.py
  - security_tester.py
  - benchmark_compression.py
  - Modified: 3 files (flask_server_hivemq.py, server_security_layer.py, firmware_manager.py)

- **Tests**: 8 files
  - test_protocol.py
  - test_compression.py
  - test_compression_crossval.py
  - test_security.py
  - test_security_replay.py
  - test_security_tamper.py
  - test_fota.py
  - test_integration.py

- **Configuration**: 1 file
  - justfile

**Total Flask**: 12 new files + 3 modified

---

#### Documentation
- **Planning Documents**: 5 files
  - IMPROVEMENT_PLAN.md (updated)
  - TODO_CHECKLIST.md (updated)
  - ARCHITECTURE_IMPROVEMENTS.md (new)
  - IMPROVEMENT_SUMMARY.md (new)
  - UPDATE_SUMMARY.md (new)
  - FILE_STRUCTURE.md (this file, new)

**Total Documentation**: 6 files

---

### Grand Total
- **New Files**: 39
- **Modified Files**: 6
- **Total Lines of Code**: ~5,500 new lines
- **Estimated Time**: 15-21 hours

---

## 🎯 Implementation Order

### Phase 1: Setup (30 min)
```
✅ Install Just: cargo install just
✅ Create test directories
✅ Create configuration files (test_config.h, wokwi.toml, diagram.json)
✅ Copy Justfiles to both directories
```

### Phase 2: Diagnostics (4 hours)
```
ESP32:
1. diagnostics.h/cpp
2. Modify protocol_adapter.cpp (add validation)
3. test_diagnostics_unit.cpp
4. test_protocol_diagnostics.cpp

Flask:
1. fault_injector.py
2. Modify flask_server_hivemq.py (add endpoints)
3. test_protocol.py

Test: just test-diagnostics (ESP32)
      just test-protocol (Flask)
```

### Phase 3: Aggregation (2 hours)
```
ESP32:
1. aggregation.h/cpp
2. test_aggregation_unit.cpp
3. Modify compression.cpp (integrate aggregation)

Flask:
1. Modify flask_server_hivemq.py (deaggregation)
2. test_compression.py (add aggregation tests)

Test: just test-aggregation
```

### Phase 4: Compression Validation (3 hours)
```
ESP32:
1. test_compression_methods.cpp
2. Fix statistics in main.cpp

Flask:
1. benchmark_compression.py
2. test_compression_crossval.py

Test: just test-compression (both sides)
```

### Phase 5: Security Testing (4 hours)
```
ESP32:
1. test_security_unit.cpp
2. Modify OTAManager.cpp (enhanced diagnostics)

Flask:
1. security_tester.py
2. Modify server_security_layer.py (persistent nonce)
3. test_security.py
4. test_security_replay.py
5. test_security_tamper.py
6. test_fota.py

Test: just test-security (both sides)
```

### Phase 6: Wokwi Support (2 hours)
```
ESP32:
1. wokwi_mock.h/cpp
2. Modify arduino_wifi.cpp (Wokwi support)
3. Modify protocol_adapter.cpp (use mocks)
4. Modify platformio.ini (add [env:wokwi])

Test: just build-wokwi
      F1 → "Wokwi: Start Simulator"
```

### Phase 7: Integration (2 hours)
```
Flask:
1. test_integration.py

Test: just integration-test
      just validate (both sides)
```

---

## 📖 File Purpose Quick Reference

### Core Implementation
| File | Purpose | Size |
|------|---------|------|
| `diagnostics.cpp` | Event logging, error tracking | ~200 lines |
| `aggregation.cpp` | Min/max/avg calculation | ~150 lines |
| `fault_injector.py` | Inject CRC/truncate/timeout errors | ~150 lines |
| `security_tester.py` | Attack simulation | ~200 lines |
| `benchmark_compression.py` | Python compression validator | ~250 lines |
| `wokwi_mock.cpp` | Simulator HTTP/WiFi mocks | ~100 lines |

### Testing
| File | Purpose | Coverage |
|------|---------|----------|
| `test_diagnostics_unit.cpp` | Event logging, NVS persistence | Diagnostics |
| `test_aggregation_unit.cpp` | Min/max/avg correctness | Aggregation |
| `test_compression_methods.cpp` | All compression methods | Compression |
| `test_security_unit.cpp` | HMAC, nonce handling | Security |
| `test_compression_crossval.py` | ESP32 ↔ Python match | Cross-validation |
| `test_integration.py` | End-to-end full cycle | Integration |

### Automation
| File | Purpose | Commands |
|------|---------|----------|
| `PIO/ECOWATT/justfile` | ESP32 build automation | 40+ recipes |
| `flask/justfile` | Flask server automation | 50+ recipes |
| `wokwi.toml` | Wokwi configuration | Simulation |
| `diagram.json` | Wokwi circuit diagram | Visualization |

---

## ✅ Verification Checklist

After implementation, verify all files exist:

### ESP32
```bash
cd PIO/ECOWATT
ls -la justfile wokwi.toml diagram.json
ls -la src/application/diagnostics.*
ls -la src/application/aggregation.*
ls -la src/driver/wokwi_mock.*
ls -la test/test_*.cpp
```

### Flask
```bash
cd flask
ls -la justfile
ls -la fault_injector.py security_tester.py benchmark_compression.py
ls -la tests/test_*.py
```

### Functionality
```bash
# ESP32
cd PIO/ECOWATT
just --list              # Should show 40+ commands
just test-all            # Should run all tests

# Flask
cd flask
just --list              # Should show 50+ commands
just test-all            # Should run all tests

# Integration
just validate            # Both sides
```

---

## 🆘 Troubleshooting

### "command not found: just"
```bash
# Install Just
cargo install just
# Or
apt install just
# Or
brew install just
```

### "No test files found"
```bash
# Verify test directory exists
ls -la PIO/ECOWATT/test/
# Should contain test_*.cpp files
```

### "Wokwi simulator not starting"
```bash
# Check extension installed
# VSCode → Extensions → Search "Wokwi"
# Verify wokwi.toml paths correct
cat PIO/ECOWATT/wokwi.toml
```

### "Import error in Python tests"
```bash
# Install test dependencies
cd flask
pip install pytest pytest-watch
```

---

**File structure complete!** Use this as reference during implementation. 📁✨
