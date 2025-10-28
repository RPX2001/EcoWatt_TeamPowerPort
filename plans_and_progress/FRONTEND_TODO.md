# EcoWatt Frontend Development TODO List

## ✅ = Completed | 🔄 = In Progress | ⏳ = Not Started

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

## Phase 4: Logging & Monitoring 🎯

### Log Viewer
- [ ] ⏳ Create `src/api/diagnostics.js`
- [ ] ⏳ Create `LogViewer.jsx`
  - [ ] Scrollable log display
  - [ ] Color coding by severity (info/warning/error)
  - [ ] Auto-scroll to bottom toggle
  - [ ] Search functionality
- [ ] ⏳ Create `LogFilters.jsx`
  - [ ] Filter by device
  - [ ] Filter by log level
  - [ ] Filter by date range
  - [ ] Clear filters button
- [ ] ⏳ Add log export functionality (CSV/JSON)

### Statistics Dashboard
- [ ] ⏳ Create `StatisticsCard.jsx` component
- [ ] ⏳ Create statistics page layout
  - [ ] Compression stats section
  - [ ] Security stats section
  - [ ] OTA stats section
  - [ ] Command stats section
- [ ] ⏳ Add charts for trends
- [ ] ⏳ Create `Logs.jsx` page

### Diagnostics
- [ ] ⏳ Create `DiagnosticsSummary.jsx`
- [ ] ⏳ Display device health metrics
- [ ] ⏳ Show system health (server uptime, API response time)
- [ ] ⏳ Test with real diagnostic data

### Estimated Time: 4-6 days

---

## Phase 5: Utilities 🎯

### Firmware Preparation Tool
- [ ] ⏳ Create `FirmwarePrep.jsx`
  - [ ] Firmware file uploader
  - [ ] Version input
  - [ ] Algorithm selector (if multiple)
  - [ ] Generate manifest button
  - [ ] Display generated manifest
  - [ ] Download prepared files
- [ ] ⏳ Create Flask endpoint to execute `prepare_firmware.py`
- [ ] ⏳ Test firmware preparation workflow

### Key Generation Tool
- [ ] ⏳ Create `KeyGenerator.jsx`
  - [ ] Key type selector (PSK/HMAC)
  - [ ] Key format selector (C header/PEM/binary)
  - [ ] Generate button
  - [ ] Display generated keys
  - [ ] Copy to clipboard button
  - [ ] Download as file
- [ ] ⏳ Create Flask endpoint to execute `generate_keys.py`
- [ ] ⏳ Test key generation

### Compression Benchmark
- [ ] ⏳ Create `CompressionBench.jsx`
  - [ ] Start benchmark button
  - [ ] Algorithm comparison table
  - [ ] Compression ratio chart
  - [ ] Speed comparison chart
  - [ ] Memory usage display
- [ ] ⏳ Create Flask endpoint to execute `benchmark_compression.py`
- [ ] ⏳ Add result visualization
- [ ] ⏳ Test benchmark execution

### Utilities Page
- [ ] ⏳ Create `Utilities.jsx` page layout
- [ ] ⏳ Organize utilities into tabs/sections
- [ ] ⏳ Test all utility tools

### Estimated Time: 4-6 days

---

## Phase 6: Testing Features 🎯

### Fault Injection Interface
- [ ] ⏳ Create `src/api/fault.js`
- [ ] ⏳ Create `FaultInjection.jsx`
  - [ ] Fault type selector dropdown
    * Network timeout
    * Malformed data
    * Buffer overflow
    * CRC error
  - [ ] Device selector
  - [ ] Inject button
  - [ ] Status display
  - [ ] Result log viewer
- [ ] ⏳ Add Flask endpoints for fault injection
- [ ] ⏳ Test fault scenarios

### Security Testing
- [ ] ⏳ Create `SecurityTests.jsx`
  - [ ] Replay attack test
  - [ ] Tampered payload test
  - [ ] Invalid HMAC test
  - [ ] Old nonce test
  - [ ] Test result display
  - [ ] Security stats viewer
- [ ] ⏳ Add Flask endpoints for security testing
- [ ] ⏳ Test security scenarios

### Upload Error Testing
- [ ] ⏳ Create `UploadTests.jsx`
  - [ ] Simulate upload failure
  - [ ] Test retry logic
  - [ ] Verify backoff behavior
  - [ ] Display retry attempts
- [ ] ⏳ Test upload error scenarios

### OTA Testing
- [ ] ⏳ Create `OTATests.jsx`
  - [ ] Test successful update
  - [ ] Test failed update with rollback
  - [ ] Test hash mismatch
  - [ ] Test chunked download errors
  - [ ] Display test results
- [ ] ⏳ Test OTA scenarios

### Testing Page
- [ ] ⏳ Create `Testing.jsx` page layout
- [ ] ⏳ Organize tests into sections
- [ ] ⏳ Add "Run All Tests" functionality
- [ ] ⏳ Test complete testing suite

### Estimated Time: 4-6 days

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
