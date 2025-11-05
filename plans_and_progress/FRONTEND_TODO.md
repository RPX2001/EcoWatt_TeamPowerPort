# EcoWatt Frontend Development TODO List

## ✅ = Completed | 🔄 = In Progress | ⏳ = Not Started

---

## Recent Fix (Nov 5, 2025) - Compression Bug ✅ COMPLETED

### Issue: Bit-Packing Decompression Mismatch
- [x] ✅ Fixed header order: ESP32 sends [marker][bits][count], Flask was reading [marker][count][bits]
- [x] ✅ Fixed bit unpacking: Changed from LSB-first to MSB-first to match ESP32's packing algorithm  
- [x] ✅ Verified: Modbus values (Vac1=2384, Iac1=149, Pac=3596) now decompress correctly in Flask
- [x] ✅ Added support for 0x70/0x71 temporal compression markers (for future use)

---

## Pre-Development Setup ✅ COMPLETED

### Environment & Tools
- [x] ✅ Choose UI framework (Material-UI)
- [x] ✅ Decide on charting library (Recharts)
- [x] ✅ Confirm deployment strategy (Flask static)
- [x] ✅ Install Node.js and npm/yarn
- [x] ✅ Initialize Vite + React project
- [x] ✅ Install dependencies (axios, react-router, UI library, charts)
- [x] ✅ Setup ESLint and Prettier
- [x] ✅ Configure CORS in Flask for frontend development

### Flask Backend Enhancements
- [x] ✅ Add missing endpoints (see FRONTEND_PLAN.md)
- [x] ✅ Implement `/devices` endpoint for device management
- [x] ✅ Add `/ota/upload` endpoint for firmware upload
- [x] ✅ Create `/fault/inject` endpoint for testing
- [x] ✅ Add `/export/<device_id>/csv` endpoint
- [x] ✅ Add `/aggregation/latest/<device_id>` endpoint
- [x] ✅ Add `/aggregation/historical/<device_id>` endpoint
- [x] ✅ Test all endpoints - Backend returning correct data ✓

---

## Phase 1: Setup & Core Dashboard ✅ COMPLETED

### Project Initialization
- [x] ✅ Create Vite project in `front-end` folder
- [x] ✅ Install UI framework: Material-UI
- [x] ✅ Install routing: react-router-dom
- [x] ✅ Install HTTP client: axios
- [x] ✅ Install charting library: recharts
- [x] ✅ Install icons: @mui/icons-material
- [x] ✅ Install date utilities: date-fns
- [x] ✅ Install React Query: @tanstack/react-query
- [x] ✅ Install Socket.IO client: socket.io-client

### Basic Structure
- [x] ✅ Create folder structure (api, components, pages, etc.)
- [x] ✅ Setup Vite config with Flask proxy
- [x] ✅ Create axios configuration
- [x] ✅ Create WebSocket configuration
- [x] ✅ Create MUI theme
- [x] ✅ Setup test configuration
- [x] ✅ Add testing scripts to package.json
- [x] ✅ Create initial App.jsx with providers
- [x] ✅ Create justfile for common commands
- [x] ✅ Verify setup works

### Basic Layout
- [x] ✅ Create `src/components/common/Navbar.jsx`
- [x] ✅ Create `src/components/common/Sidebar.jsx`
- [x] ✅ Create `src/components/common/Footer.jsx`
- [x] ✅ Setup routing in `App.jsx`
- [x] ✅ Create page placeholders (Dashboard, Config, Commands, etc.)

### API Integration
- [x] ✅ Create `src/api/axios.js` with base configuration
- [x] ✅ Create `src/api/devices.js` with device API calls
- [x] ✅ Create `src/api/aggregation.js` for data fetching
- [x] ✅ Create API Tester utility component

### Dashboard Components
- [x] ✅ Create `DeviceSelector.jsx` (dropdown to select device)
- [x] ✅ Create `MetricsCard.jsx` (display single metric)
- [x] ✅ Create `TimeSeriesChart.jsx` (voltage/current over time)
- [x] ✅ Create `Dashboard.jsx` page layout
- [x] ✅ Implement auto-refresh (5-second interval)
- [x] ✅ Add loading states and error handling
- [x] ✅ Ready to test with real data from Flask

### Estimated Time: 5-7 days

---

## Phase 2: Configuration & Commands ✅ COMPLETED

### Configuration Management
- [x] ✅ Create `src/api/config.js` for config API calls
- [x] ✅ Create `ConfigForm.jsx` with form validation
  - [x] ✅ Sample rate input (Hz)
  - [x] ✅ Upload interval input (seconds)
  - [x] ✅ Register list selector (checkboxes)
  - [x] ✅ Compression toggle
  - [x] ✅ Power threshold input
  - [x] ✅ Updated with 9 Modbus registers (0-7, 9)
  - [x] ✅ Added 5 timing parameters (Milestone 4)
- [x] ✅ Create `ConfigHistory.jsx` to view past configs with diff view
- [x] ✅ Create `Configuration.jsx` page with tabs
- [x] ✅ Add save/reset functionality
- [x] ✅ Test config updates with Milestone 4 format

### Command Execution
- [x] ✅ Create `src/api/commands.js` for command API calls
- [x] ✅ Create `CommandBuilder.jsx`
  - [x] ✅ Command type dropdown (set_power, write_register, update_config)
  - [x] ✅ Dynamic parameter inputs based on command type
  - [x] ✅ Parameter validation
  - [x] ✅ Focus on Register 8 (Export Power % 0-100%)
- [x] ✅ Create `CommandQueue.jsx` to show pending commands with auto-refresh (10s)
- [x] ✅ Create `CommandHistory.jsx` table
  - [x] ✅ Columns: timestamp, device, command, status, result
  - [x] ✅ Sortable and filterable with pagination
- [x] ✅ Create `Commands.jsx` page layout with tabs
- [x] ✅ Test command execution flow with Milestone 4 format

### Navigation & UI Integration
- [x] ✅ Implement URL parameter navigation (?tab=send, ?tab=queue, ?tab=history)
- [x] ✅ Update Sidebar with collapsible sub-menus
- [x] ✅ Fix navigation to specific tabs from sidebar
- [x] ✅ Integrate Footer component in App.jsx

### Fault Injection
- [x] ✅ Create `src/api/faults.js` with dual backend support
- [x] ✅ Update flask/routes/fault_routes.py for Inverter SIM API integration
- [x] ✅ Add fault presets (EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY)
- [x] ✅ Local fault support (network, mqtt, command, ota)

### Estimated Time: 5-7 days → ✅ COMPLETED

---

## Phase 3: FOTA Management ✅ COMPLETED

### Firmware Upload
- [x] ✅ Create `src/api/ota.js` for OTA API calls
- [x] ✅ Create `FirmwareUpload.jsx`
  - [x] ✅ File upload (drag & drop + browse)
  - [x] ✅ Version input field
  - [x] ✅ File validation (.bin, .hex, .elf)
  - [x] ✅ Upload progress bar
- [x] ✅ Flask endpoints already exist in flask/routes/ota_routes.py
- [ ] ⏳ Test firmware file upload

### OTA Management
- [x] ✅ Create `FirmwareList.jsx` to display available versions
  - [x] ✅ Version, size, date uploaded
  - [x] ✅ Delete button
  - [x] ✅ Initiate OTA button
  - [x] ✅ View manifest button
  - [x] ✅ OTA initiation dialog with device selector
- [x] ✅ Create `OTAProgress.jsx`
  - [x] ✅ Device selector for OTA
  - [x] ✅ Progress bar for chunk download
  - [x] ✅ Status indicators (idle, downloading, verifying, installing, completed, failed)
  - [x] ✅ Success/failure notification
  - [x] ✅ Cancel OTA button
  - [x] ✅ OTA statistics display
  - [x] ✅ Real-time status polling (2s interval)
- [x] ✅ Create `FOTA.jsx` page layout with tabs
  - [x] ✅ Tab 1: Upload Firmware
  - [x] ✅ Tab 2: Manage Firmware
  - [x] ✅ Tab 3: Update Progress
  - [x] ✅ URL parameter navigation (?tab=upload, ?tab=manage, ?tab=progress)
- [ ] ⏳ Test OTA workflow end-to-end

### Estimated Time: 5-7 days → ✅ COMPLETED (Implementation phase done, testing pending)

---

## Phase 4: Logging & Monitoring ✅ COMPLETED

### Log Viewer
- [x] ✅ Create `src/api/diagnostics.js`
  - [x] ✅ getAllDiagnostics, getDeviceDiagnostics
  - [x] ✅ storeDiagnostics, clearDiagnostics
  - [x] ✅ getDiagnosticsSummary
  - [x] ✅ getAllStats, getCompressionStats, getSecurityStats, getOTAStats, getCommandStats
  - [x] ✅ getSystemHealth
- [x] ✅ Create `LogViewer.jsx`
  - [x] ✅ Scrollable log display with max height
  - [x] ✅ Color coding by severity (INFO, WARNING, ERROR, DEBUG)
  - [x] ✅ Auto-scroll to bottom toggle
  - [x] ✅ Search functionality across all log fields
  - [x] ✅ Real-time updates with auto-refresh toggle (5s interval)
  - [x] ✅ Export to CSV and JSON formats
  - [x] ✅ Clear logs functionality
  - [x] ✅ Log count display
  - [x] ✅ Emoji icons for log levels
- [x] ✅ Create `LogFilters.jsx`
  - [x] ✅ Filter by device with dropdown
  - [x] ✅ Filter by log level (ALL, INFO, WARNING, ERROR, DEBUG)
  - [x] ✅ Filter by date range (start and end datetime)
  - [x] ✅ Search text filter
  - [x] ✅ Clear all filters button with active filter count badge
  - [x] ✅ Active filters display as removable chips
- [x] ✅ Add log export functionality (CSV/JSON) ✅

### Statistics Dashboard
- [x] ✅ Create `StatisticsCard.jsx` component
  - [x] ✅ Icon display with avatar
  - [x] ✅ Title and value display
  - [x] ✅ Trend indicators (up/down)
  - [x] ✅ Color coding by metric type
  - [x] ✅ Optional subtitle
- [x] ✅ Create statistics page layout
  - [x] ✅ Compression stats section (total messages, compressed, avg ratio, savings)
  - [x] ✅ Security stats section (verified messages, failed, success rate, active keys)
  - [x] ✅ OTA stats section (total updates, successful, failed, active sessions)
  - [x] ✅ Command stats section (total, successful, failed, pending)
- [x] ✅ Create `Logs.jsx` page with tabs
  - [x] ✅ Tab 1: Overview - All statistics dashboard
  - [x] ✅ Tab 2: System Health - DiagnosticsSummary
  - [x] ✅ Tab 3: Logs - Placeholder for future log viewer

### Diagnostics
- [x] ✅ Create `DiagnosticsSummary.jsx`
  - [x] ✅ Health status indicator (healthy/warning/critical)
  - [x] ✅ Device health metrics (total records, errors, warnings, last update)
  - [x] ✅ System uptime display
  - [x] ✅ API response time
  - [x] ✅ Active devices count
  - [x] ✅ Recent issues list
  - [x] ✅ Real-time polling (10s for health, 30s for summary)
- [x] ✅ Integrated LogViewer into Logs page Tab 3

### Estimated Time: 4-6 days → ✅ COMPLETED

---

## Phase 5: Utilities ✅ COMPLETED

### Firmware Preparation Tool
- [x] ✅ Create `FirmwarePrep.jsx`
  - [x] ✅ Firmware file uploader (drag & drop + browse)
  - [x] ✅ Version input with validation
  - [x] ✅ Algorithm selector (ZLIB, GZIP, LZ4, None)
  - [x] ✅ Generate manifest button
  - [x] ✅ Display generated manifest (JSON pretty-print)
  - [x] ✅ Download prepared files
  - [x] ✅ File size display
  - [x] ✅ Preparation log output
- [x] ✅ Create Flask endpoint `/utilities/firmware/prepare`
- [x] ✅ Integrated with `prepare_firmware.py` script

### Key Generation Tool
- [x] ✅ Create `KeyGenerator.jsx`
  - [x] ✅ Key type selector (PSK/HMAC/AES)
  - [x] ✅ Key format selector (C header/PEM/HEX/Base64)
  - [x] ✅ Key size selector (128/192/256/512-bit)
  - [x] ✅ Generate button with loading state
  - [x] ✅ Display generated keys in formatted view
  - [x] ✅ Copy to clipboard button (per file)
  - [x] ✅ Download individual files
  - [x] ✅ Download all files at once
  - [x] ✅ Security warnings and usage instructions
- [x] ✅ Create Flask endpoint `/utilities/keys/generate`
- [x] ✅ Integrated with `generate_keys.py` script

### Compression Benchmark
- [x] ✅ Create `CompressionBench.jsx`
  - [x] ✅ Start benchmark button
  - [x] ✅ Configurable test parameters (data size, iterations)
  - [x] ✅ Algorithm comparison table
  - [x] ✅ Compression ratio display
  - [x] ✅ Speed comparison metrics
  - [x] ✅ Summary cards for each algorithm
  - [x] ✅ Detailed results table
- [x] ✅ Create Flask endpoint `/utilities/compression/benchmark`
- [x] ✅ Integrated with `benchmark_compression.py` script

### Utilities Page
- [x] ✅ Create `Utilities.jsx` page layout
- [x] ✅ Four-tab interface (Firmware Prep, Key Generator, Compression Bench, API Tester)
- [x] ✅ Organize utilities into tabs with icons
- [x] ✅ Created Flask route `utilities_routes.py`
- [x] ✅ Registered utilities blueprint in Flask app
- [x] ✅ Created utilities API client (`utilities.js`)

### Estimated Time: 4-6 days → ✅ COMPLETED

---

## Phase 6: Testing Features ✅ COMPLETED

### Fault Injection Interface
- [x] ✅ Create `src/api/fault.js` (already exists from Phase 2)
- [x] ✅ Create `FaultInjection.jsx`
  - [x] ✅ Fault type selector dropdown
    * Inverter SIM API: EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY
    * Local Backend: Network timeout, MQTT disconnect, Command failure, OTA failure, Memory error
  - [x] ✅ Backend selector (Inverter SIM vs Local)
  - [x] ✅ Device selector
  - [x] ✅ Inject button with loading state
  - [x] ✅ Status display with color-coded results
  - [x] ✅ Result log viewer with fault history (last 10)
  - [x] ✅ Clear faults functionality
  - [x] ✅ Available fault types reference
- [x] ✅ Integrated with existing Flask fault endpoints
- [x] ✅ Integrated with Inverter SIM API (20.15.114.131:8080)

### Security Testing
- [x] ✅ Create `src/api/security.js`
  - [x] ✅ validateSecuredPayload, getSecurityStats, resetSecurityStats
  - [x] ✅ clearDeviceNonces, clearAllNonces, getDeviceSecurityInfo
  - [x] ✅ testReplayAttack, testTamperedPayload, testInvalidHMAC, testOldNonce
- [x] ✅ Create `SecurityTests.jsx`
  - [x] ✅ Device selector for targeting tests
  - [x] ✅ Test payload editor with regenerate functionality
  - [x] ✅ Run all security tests button
  - [x] ✅ Individual test execution:
    * Replay attack detection test
    * Tampered payload detection test
    * Invalid HMAC detection test
    * Old nonce rejection test
  - [x] ✅ Test result display with pass/fail indicators
  - [x] ✅ Test duration tracking
  - [x] ✅ Security statistics dashboard (total validations, successful, failed, replay attacks)
  - [x] ✅ Device security information display (nonces, last validation)
  - [x] ✅ Clear nonces functionality
  - [x] ✅ Reset stats functionality
  - [x] ✅ Test descriptions panel
- [x] ✅ Integrated with Flask security endpoints
- [x] ✅ Real-time statistics updates (10s interval)

### Testing Page
- [x] ✅ Create `Testing.jsx` page layout
- [x] ✅ Three-tab interface with icons:
  * Tab 1: Fault Injection
  * Tab 2: Security Tests
  * Tab 3: System Tests
- [x] ✅ Warning alert for testing environment usage
- [x] ✅ Consistent design with other pages (FOTA, Utilities, Logs)

### System Tests
- [x] ✅ Create `SystemTests.jsx` component
  - [x] ✅ OTA Update Test (firmware initiation, progress monitoring, completion)
  - [x] ✅ Command Execution Test (send command, verify execution)
  - [x] ✅ Configuration Update Test (get config, update, verify)
  - [x] ✅ Data Upload Test (upload simulation, error handling)
  - [x] ✅ End-to-End Workflow Test (multi-step integration test with 4 steps)
  - [x] ✅ Run All Tests functionality
  - [x] ✅ Individual test execution buttons
  - [x] ✅ Test result display with accordions
  - [x] ✅ Status indicators (pass/fail/error/timeout)
  - [x] ✅ Duration tracking for each test
  - [x] ✅ Workflow step visualization
  - [x] ✅ Test history (last 20 results)
  - [x] ✅ Clear results functionality

### Files Created/Updated
1. **front-end/src/api/security.js** (100 lines)
   - 15 API functions for security testing and validation
   
2. **front-end/src/components/testing/FaultInjection.jsx** (430 lines)
   - Dual backend support (Inverter SIM API + Local)
   - 5 Inverter SIM fault types + 5 Local fault types
   - Device targeting
   - Fault history tracking (last 10 injections)
   - Color-coded status display
   - Available fault types reference panel
   
3. **front-end/src/components/testing/SecurityTests.jsx** (550+ lines)
   - 4 comprehensive security tests
   - Test payload editor
   - Real-time statistics dashboard (4 metrics)
   - Device security info viewer
   - Test results table with duration tracking
   - Security stats management (reset/clear nonces)
   
4. **front-end/src/components/testing/SystemTests.jsx** (700+ lines) ✨ NEW
   - 5 comprehensive system tests with individual cards
   - Run all tests functionality
   - Test result accordions with detailed information
   - Workflow step visualization with status icons
   - Status tracking (pass/fail/error/timeout)
   - Test history with timestamps and expandable details
   - Device selector integration
   - Progress indicators for running tests
   
5. **front-end/src/pages/Testing.jsx** (Updated - 140 lines)
   - Tab-based navigation (3 fully functional tabs)
   - Warning alerts
   - SystemTests component integrated

### Estimated Time: 4-6 days → ✅ COMPLETED (2025-10-28)

---

## Phase 7: Polish & Integration 🎯

### UI/UX Improvements
- [ ] ⏳ Make all pages responsive (mobile/tablet)
- [ ] ⏳ Add consistent loading states (skeletons/spinners)
- [ ] ⏳ Implement proper error boundaries
- [ ] ⏳ Add toast notifications for success/error
- [ ] ⏳ Improve color scheme and branding
- [ ] ⏳ Add dark mode toggle (optional)

### Performance Optimization
- [ ] ⏳ Implement React.memo for expensive components
- [ ] ⏳ Add lazy loading for routes
- [ ] ⏳ Optimize API calls (caching, debouncing)
- [ ] ⏳ Minimize bundle size

### Documentation
- [ ] ⏳ Write README.md for frontend
- [ ] ⏳ Add inline code comments
- [ ] ⏳ Create user guide (how to use each feature)
- [ ] ⏳ Document API integration points

### Testing
- [ ] ⏳ Manual testing of all features
- [ ] ⏳ Cross-browser testing
- [ ] ⏳ Mobile responsiveness testing
- [ ] ⏳ API error handling testing
- [ ] ⏳ End-to-end workflow testing

### Deployment
- [ ] ⏳ Build production bundle: `npm run build`
- [ ] ⏳ Configure Flask to serve static files
- [ ] ⏳ Test deployed version
- [ ] ⏳ Setup environment variables

### Estimated Time: 5-7 days

---

## Optional Enhancements 🌟

### Advanced Features
- [ ] ⏳ Implement WebSocket for real-time updates
- [ ] ⏳ Add user authentication (login/logout)
- [ ] ⏳ Implement role-based access control
- [ ] ⏳ Add alert/notification system
- [ ] ⏳ Create data comparison tool (compare devices)
- [ ] ⏳ Add data export in multiple formats
- [ ] ⏳ Implement scheduled reports

### Analytics
- [ ] ⏳ Add Google Analytics or similar
- [ ] ⏳ Track user interactions
- [ ] ⏳ Monitor performance metrics

### Testing
- [ ] ⏳ Setup Jest for unit tests
- [ ] ⏳ Add Cypress for E2E tests
- [ ] ⏳ Write tests for critical components

---

## Total Estimated Time: 6-8 weeks

**Notes:**
- Adjust timeline based on team size and availability
- Some phases can overlap (e.g., testing while building features)
- Prioritize core features first, then utilities, then testing tools
- Keep stakeholders updated with demo videos at each phase

---

## Dependencies & Blockers

### Must Have Before Starting
1. ✅ Flask backend with all test endpoints working
2. ⏳ Decision on UI framework (Material-UI vs Tailwind)
3. ⏳ Flask endpoints for utilities (firmware prep, key gen, benchmark)
4. ⏳ Clear understanding of data formats from ESP32

### Nice to Have
1. ⏳ Real ESP32 device for live testing
2. ⏳ Sample data for testing UI without backend
3. ⏳ Design mockups or wireframes

---

## ✅ Decisions Made

1. **UI Framework**: ✅ Material-UI (MUI)
2. **Real-time Updates**: ✅ WebSocket (Socket.IO)
3. **Authentication**: ✅ No authentication needed
4. **Deployment**: ✅ Serve from Flask static folder
5. **Testing**: ✅ Automated tests (Jest + React Testing Library)
6. **Mobile Support**: ✅ Desktop-only (no mobile responsive needed)
7. **Charting**: ✅ Recharts
8. **State Management**: ✅ Context API + React Query

---

**Last Updated**: 2025-10-28
**Status**: Ready to Start Implementation
**Next Step**: Initialize Vite project and install dependencies
