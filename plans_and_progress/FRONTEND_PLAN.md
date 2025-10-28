# EcoWatt Frontend Development Plan

## Tech Stack Selection

### Recommended Stack
- **Framework**: React 18 with Vite
- **UI Library**: Material-UI (MUI) or Tailwind CSS + Headless UI
- **State Management**: React Context API + React Query (for API calls)
- **Charts**: Recharts or Chart.js
- **HTTP Client**: Axios
- **Real-time Updates**: Socket.IO (optional for live logs)

### Why This Stack?
1. **Lightweight**: Vite provides fast builds and hot-reload
2. **Flask Compatible**: Easy CORS setup, RESTful API integration
3. **Modern**: React hooks, component-based architecture
4. **Rich Ecosystem**: Many libraries for charts, forms, tables
5. **Easy Deployment**: Can be served as static files from Flask

---

## Feature Requirements Analysis

### 1. Dashboard Features (Core)

#### A. Register Monitoring Dashboard
**Requirements from Milestones 2-5:**
- Display voltage, current, power readings
- Show frequency, phase angle, active power (M3)
- Real-time or near-real-time updates (15-minute upload cycle simulation)
- Historical data visualization (line charts)
- Multiple device support

**API Endpoints Needed:**
- `GET /aggregated/<device_id>` - Fetch latest data
- `GET /aggregated/<device_id>/history?from=<timestamp>&to=<timestamp>` - Historical data
- `GET /devices` - List all devices (NEW endpoint needed)

**UI Components:**
- Live metrics cards (current values)
- Time-series charts (voltage/current over time)
- Device selector dropdown
- Auto-refresh toggle

#### B. Configuration Management
**Requirements from M4:**
- Update sampling interval (poll frequency)
- Update register list (which registers to read)
- Update upload frequency
- Enable/disable compression
- Power threshold settings

**API Endpoints:**
- `GET /config?device_id=<device_id>` - Get current config
- `POST /commands/<device_id>` - Send config update command
  ```json
  {
    "command": "update_config",
    "parameters": {
      "sample_rate_hz": 10,
      "upload_interval_s": 60,
      "compression_enabled": true,
      "power_threshold_w": 1000
    }
  }
  ```

**UI Components:**
- Configuration form with validation
- Save/Apply buttons
- Config history viewer
- Pending changes indicator

#### C. Command Execution Interface
**Requirements from M4:**
- Send write_register commands to inverter
- Send set_power commands
- View command status (pending/executing/completed)
- Command history log

**API Endpoints:**
- `POST /commands/<device_id>` - Queue command
- `GET /commands/<device_id>/poll` - View pending commands
- `GET /commands/status/<command_id>` - Check command status
- `GET /commands/stats` - Command statistics

**UI Components:**
- Command builder form (select register/value)
- Pre-defined command templates (common operations)
- Command queue viewer
- Execution result display
- Command history table

#### D. FOTA Management
**Requirements from M4-M5:**
- Upload firmware binary
- View available firmware versions
- Initiate OTA update for device(s)
- Monitor OTA progress
- View update history
- Rollback capability

**API Endpoints:**
- `POST /ota/upload` - Upload new firmware (NEW endpoint needed)
- `GET /ota/check/<device_id>?version=<version>` - Check for updates
- `POST /ota/initiate/<device_id>` - Start OTA session
- `GET /ota/status/<device_id>` - Monitor progress
- `GET /ota/stats` - Overall OTA statistics
- `POST /ota/cancel/<device_id>` - Cancel OTA (if needed)

**UI Components:**
- Firmware upload form (with manifest generation)
- Available versions list
- Device selection for OTA
- Progress bars for chunk download
- Success/failure indicators
- Rollback button for failed updates

#### E. Logging & Monitoring
**Requirements from M2-M5:**
- View all data received from ESP32
- Server-side event logs
- Security validation logs
- Compression statistics
- Diagnostic information

**API Endpoints:**
- `GET /diagnostics` - All diagnostics
- `GET /diagnostics/<device_id>` - Device-specific diagnostics
- `POST /diagnostics/<device_id>` - Log diagnostic event (NEW)
- `GET /compression/stats` - Compression statistics
- `GET /security/stats` - Security statistics
- `GET /ota/stats` - OTA statistics
- `GET /commands/stats` - Command statistics

**UI Components:**
- Live log viewer (scrollable, filterable)
- Log export functionality (CSV/JSON)
- Statistics dashboard
- Diagnostic summary cards

---

### 2. Utility Features

#### A. Firmware Preparation Tool
**Requirements from M4:**
- Generate firmware manifest (version, size, hash)
- Encrypt firmware (optional)
- Sign firmware with HMAC
- Package for FOTA

**Implementation:**
- Use existing `prepare_firmware.py` script
- Create UI wrapper to execute script
- Display results and generated files

**UI Components:**
- Firmware file uploader
- Version input field
- Manifest preview
- Generate button
- Download prepared firmware + manifest

#### B. Key Generation Tool
**Requirements from M4:**
- Generate PSK keys
- Generate HMAC keys
- Export keys in various formats (C header, binary, PEM)

**Implementation:**
- Use existing `generate_keys.py` script
- Create UI wrapper

**UI Components:**
- Key type selector (PSK/HMAC)
- Key format selector
- Generate button
- Copy to clipboard functionality
- Download as file

#### C. Compression Benchmark
**Requirements from M3:**
- Run compression benchmarks
- Compare different algorithms
- Show compression ratios
- Display speed metrics

**Implementation:**
- Use existing `benchmark_compression.py` script
- Create UI to visualize results

**UI Components:**
- Start benchmark button
- Algorithm comparison table
- Compression ratio chart
- Speed comparison chart
- Memory usage stats

---

### 3. Testing Features

#### A. Fault Injection Testing
**Requirements from M5:**
- Inject network errors
- Simulate malformed data
- Test timeout handling
- Trigger buffer overflow scenarios

**API Endpoints:**
- `POST /fault/test/enable` - Enable fault injection
- `POST /fault/test/disable` - Disable fault injection
- `GET /fault/test/status` - Check fault status
- `POST /fault/inject` - Trigger specific fault (NEW endpoint needed)

**UI Components:**
- Fault type selector (network/timeout/malformed/overflow)
- Inject fault button
- Fault status indicator
- Test result display

#### B. Upload Error Testing
**Requirements from M5:**
- Simulate upload failures
- Test retry logic
- Verify backoff behavior

**API Endpoints:**
- `POST /fault/upload/simulate-failure` (NEW)
- `GET /diagnostics/<device_id>` - View upload failures

**UI Components:**
- Failure scenario selector
- Trigger button
- Retry attempt counter
- Success/failure log

#### C. Security Testing
**Requirements from M4:**
- Test replay attacks
- Test tampered data
- Test invalid HMAC
- Test old nonce

**API Endpoints:**
- `POST /security/test/replay` (NEW)
- `POST /security/test/tamper` (NEW)
- `GET /security/stats` - View blocked attacks

**UI Components:**
- Attack type selector
- Execute attack button
- Security log viewer
- Attack statistics

#### D. FOTA Testing
**Requirements from M4-M5:**
- Test successful update
- Test failed update with rollback
- Test hash mismatch
- Test chunked download errors

**API Endpoints:**
- `POST /ota/test/enable` - Enable OTA fault injection
- `POST /ota/test/disable` - Disable OTA fault injection
- `GET /ota/test/status` - Check OTA test status

**UI Components:**
- FOTA test scenario selector
- Execute test button
- Test progress monitor
- Pass/fail indicator

---

## Additional Capabilities Needed (from Milestone Analysis)

### Missing Features to Complete Demonstration

1. **Device Management**
   - Device registration
   - Device status monitoring (online/offline)
   - Last seen timestamp
   - Device health indicators

2. **Data Visualization Enhancements**
   - Comparison between multiple devices
   - Trend analysis
   - Anomaly detection visualization
   - Export data to CSV/Excel

3. **Alert System**
   - Configure thresholds for alerts
   - View active alerts
   - Alert history
   - Email/notification integration (optional)

4. **Power Optimization Metrics** (M5)
   - Display power consumption before/after optimization
   - Show DVFS settings
   - Sleep mode statistics
   - Energy savings calculator

5. **System Health Dashboard**
   - Server uptime
   - API response times
   - Database size
   - Active connections count

6. **User Authentication** (Optional but recommended)
   - Login/logout
   - Role-based access (admin/viewer)
   - API key management

---

## New Flask Endpoints Required

Based on the analysis, these endpoints need to be added:

```python
# Device Management
GET  /devices                          # List all registered devices
GET  /devices/<device_id>              # Get device details
POST /devices                          # Register new device
PUT  /devices/<device_id>              # Update device info

# Firmware Upload
POST /ota/upload                       # Upload firmware binary + manifest
GET  /ota/firmware                     # List available firmware versions
DELETE /ota/firmware/<version>         # Delete firmware version

# Advanced Diagnostics
POST /diagnostics/<device_id>          # Add diagnostic entry
GET  /diagnostics/summary              # System-wide summary

# Fault Injection (Testing)
POST /fault/inject                     # Inject specific fault type
GET  /fault/history                    # View fault injection history

# Security Testing
POST /security/test/replay             # Test replay attack
POST /security/test/tamper             # Test tampered payload

# System Stats
GET  /system/health                    # Server health metrics
GET  /system/stats                     # Overall system statistics

# Data Export
GET  /export/<device_id>/csv           # Export device data as CSV
GET  /export/<device_id>/json          # Export device data as JSON
```

---

## Project Structure

```
ecowatt-frontend/
├── public/
│   └── favicon.ico
├── src/
│   ├── api/
│   │   ├── axios.js              # Axios configuration
│   │   ├── devices.js            # Device API calls
│   │   ├── commands.js           # Command API calls
│   │   ├── ota.js                # FOTA API calls
│   │   ├── diagnostics.js        # Diagnostics API calls
│   │   └── security.js           # Security API calls
│   ├── components/
│   │   ├── common/
│   │   │   ├── Navbar.jsx
│   │   │   ├── Sidebar.jsx
│   │   │   └── Footer.jsx
│   │   ├── dashboard/
│   │   │   ├── MetricsCard.jsx
│   │   │   ├── TimeSeriesChart.jsx
│   │   │   └── DeviceSelector.jsx
│   │   ├── config/
│   │   │   ├── ConfigForm.jsx
│   │   │   └── ConfigHistory.jsx
│   │   ├── commands/
│   │   │   ├── CommandBuilder.jsx
│   │   │   ├── CommandQueue.jsx
│   │   │   └── CommandHistory.jsx
│   │   ├── ota/
│   │   │   ├── FirmwareUpload.jsx
│   │   │   ├── OTAProgress.jsx
│   │   │   └── FirmwareList.jsx
│   │   ├── logs/
│   │   │   ├── LogViewer.jsx
│   │   │   └── LogFilters.jsx
│   │   ├── utilities/
│   │   │   ├── FirmwarePrep.jsx
│   │   │   ├── KeyGenerator.jsx
│   │   │   └── CompressionBench.jsx
│   │   └── testing/
│   │       ├── FaultInjection.jsx
│   │       ├── SecurityTests.jsx
│   │       └── OTATests.jsx
│   ├── pages/
│   │   ├── Dashboard.jsx
│   │   ├── Configuration.jsx
│   │   ├── Commands.jsx
│   │   ├── FOTA.jsx
│   │   ├── Logs.jsx
│   │   ├── Utilities.jsx
│   │   └── Testing.jsx
│   ├── contexts/
│   │   └── DeviceContext.jsx     # Global device state
│   ├── hooks/
│   │   ├── useDevices.js
│   │   ├── useCommands.js
│   │   └── useWebSocket.js       # For real-time updates
│   ├── utils/
│   │   ├── formatters.js         # Data formatting utilities
│   │   └── validators.js         # Form validation
│   ├── App.jsx
│   ├── main.jsx
│   └── index.css
├── package.json
├── vite.config.js
└── README.md
```

---

## Implementation Phases

### Phase 1: Setup & Core Dashboard (Week 1)
- [ ] Initialize Vite + React project
- [ ] Setup Material-UI or Tailwind
- [ ] Create basic layout (Navbar, Sidebar, routing)
- [ ] Implement device selector
- [ ] Create metrics cards for live data
- [ ] Add time-series chart for voltage/current
- [ ] Connect to existing Flask endpoints

### Phase 2: Configuration & Commands (Week 2)
- [ ] Build configuration form
- [ ] Implement config validation
- [ ] Create command builder interface
- [ ] Add command queue viewer
- [ ] Display command history
- [ ] Show command status

### Phase 3: FOTA Management (Week 2-3)
- [ ] Create firmware upload form
- [ ] Build OTA initiation interface
- [ ] Add progress monitoring
- [ ] Implement firmware version list
- [ ] Add rollback capability

### Phase 4: Logging & Monitoring (Week 3)
- [ ] Build log viewer component
- [ ] Add log filtering
- [ ] Create statistics dashboard
- [ ] Implement log export (CSV/JSON)
- [ ] Add diagnostics summary

### Phase 5: Utilities (Week 3-4)
- [ ] Create firmware preparation UI
- [ ] Build key generation tool
- [ ] Add compression benchmark UI
- [ ] Implement result visualization

### Phase 6: Testing Features (Week 4)
- [ ] Build fault injection interface
- [ ] Create security testing tools
- [ ] Add OTA testing scenarios
- [ ] Implement test result display

### Phase 7: Polish & Integration (Week 4-5)
- [ ] Add responsive design
- [ ] Implement error handling
- [ ] Add loading states
- [ ] Create user documentation
- [ ] Perform end-to-end testing

---

## Key Questions for You

1. **UI Framework Preference**: Do you prefer Material-UI (pre-built components) or Tailwind CSS (more customization)?

2. **Real-time Updates**: Should the dashboard auto-refresh, or would you prefer Socket.IO for live updates?

3. **Authentication**: Do we need user login, or is this for demonstration only?

4. **Data Persistence**: Should frontend store any data locally (localStorage/IndexedDB), or always fetch from server?

5. **Deployment**: Will you serve the frontend from Flask (`/static/`), or deploy separately?

6. **Charting Library**: Preference for Recharts (React-native) vs Chart.js (more features)?

7. **Testing Requirements**: Do you want Cypress/Jest tests for the frontend?

8. **Mobile Support**: Should the UI be mobile-responsive?

Let me know your preferences, and I'll create the detailed implementation TODO list!
