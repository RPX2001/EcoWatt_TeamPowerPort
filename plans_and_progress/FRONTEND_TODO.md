# EcoWatt Frontend Development TODO List

## ✅ = Completed | 🔄 = In Progress | ⏳ = Not Started

---

## Pre-Development Setup ⏳

### Environment & Tools
- [ ] ⏳ Choose UI framework (Material-UI vs Tailwind CSS)
- [ ] ⏳ Decide on charting library (Recharts vs Chart.js)
- [ ] ⏳ Confirm deployment strategy (Flask static vs separate)
- [ ] ⏳ Install Node.js and npm/yarn
- [ ] ⏳ Initialize Vite + React project
- [ ] ⏳ Install dependencies (axios, react-router, UI library, charts)
- [ ] ⏳ Setup ESLint and Prettier
- [ ] ⏳ Configure CORS in Flask for frontend development

### Flask Backend Enhancements
- [ ] ⏳ Add missing endpoints (see FRONTEND_PLAN.md)
- [ ] ⏳ Implement `/devices` endpoint for device management
- [ ] ⏳ Add `/ota/upload` endpoint for firmware upload
- [ ] ⏳ Create `/fault/inject` endpoint for testing
- [ ] ⏳ Add `/export/<device_id>/csv` endpoint
- [ ] ⏳ Test all endpoints with Postman/curl

---

## Phase 1: Setup & Core Dashboard 🎯

### Project Initialization
- [ ] ⏳ Create Vite project: `npm create vite@latest ecowatt-frontend -- --template react`
- [ ] ⏳ Install UI framework: `npm install @mui/material @emotion/react @emotion/styled`
- [ ] ⏳ Install routing: `npm install react-router-dom`
- [ ] ⏳ Install HTTP client: `npm install axios`
- [ ] ⏳ Install charting library: `npm install recharts`
- [ ] ⏳ Install icons: `npm install @mui/icons-material`
- [ ] ⏳ Install date utilities: `npm install date-fns`

### Basic Layout
- [ ] ⏳ Create `src/components/common/Navbar.jsx`
- [ ] ⏳ Create `src/components/common/Sidebar.jsx`
- [ ] ⏳ Create `src/components/common/Footer.jsx`
- [ ] ⏳ Setup routing in `App.jsx`
- [ ] ⏳ Create page placeholders (Dashboard, Config, Commands, etc.)

### API Integration
- [ ] ⏳ Create `src/api/axios.js` with base configuration
- [ ] ⏳ Create `src/api/devices.js` with device API calls
- [ ] ⏳ Create `src/api/aggregation.js` for data fetching
- [ ] ⏳ Test API connections with Flask backend

### Dashboard Components
- [ ] ⏳ Create `DeviceSelector.jsx` (dropdown to select device)
- [ ] ⏳ Create `MetricsCard.jsx` (display single metric)
- [ ] ⏳ Create `TimeSeriesChart.jsx` (voltage/current over time)
- [ ] ⏳ Create `Dashboard.jsx` page layout
- [ ] ⏳ Implement auto-refresh toggle
- [ ] ⏳ Add loading states and error handling
- [ ] ⏳ Test with real data from Flask

### Estimated Time: 5-7 days

---

## Phase 2: Configuration & Commands 🎯

### Configuration Management
- [ ] ⏳ Create `src/api/config.js` for config API calls
- [ ] ⏳ Create `ConfigForm.jsx` with form validation
  - [ ] Sample rate input (Hz)
  - [ ] Upload interval input (seconds)
  - [ ] Register list selector (checkboxes)
  - [ ] Compression toggle
  - [ ] Power threshold input
- [ ] ⏳ Create `ConfigHistory.jsx` to view past configs
- [ ] ⏳ Create `Configuration.jsx` page
- [ ] ⏳ Add save/reset functionality
- [ ] ⏳ Show pending vs applied config
- [ ] ⏳ Test config updates end-to-end

### Command Execution
- [ ] ⏳ Create `src/api/commands.js` for command API calls
- [ ] ⏳ Create `CommandBuilder.jsx`
  - [ ] Command type dropdown (set_power, write_register, update_config)
  - [ ] Dynamic parameter inputs based on command type
  - [ ] Parameter validation
- [ ] ⏳ Create `CommandQueue.jsx` to show pending commands
- [ ] ⏳ Create `CommandHistory.jsx` table
  - [ ] Columns: timestamp, device, command, status, result
  - [ ] Sortable and filterable
- [ ] ⏳ Create `Commands.jsx` page layout
- [ ] ⏳ Add command status polling
- [ ] ⏳ Test command execution flow

### Estimated Time: 5-7 days

---

## Phase 3: FOTA Management 🎯

### Firmware Upload
- [ ] ⏳ Create `src/api/ota.js` for OTA API calls
- [ ] ⏳ Create `FirmwareUpload.jsx`
  - [ ] File upload (drag & drop + browse)
  - [ ] Version input field
  - [ ] Manifest preview
  - [ ] Upload progress bar
- [ ] ⏳ Add Flask endpoint for firmware upload
- [ ] ⏳ Test firmware file upload

### OTA Management
- [ ] ⏳ Create `FirmwareList.jsx` to display available versions
  - [ ] Version, size, date uploaded
  - [ ] Delete button
  - [ ] Initiate OTA button
- [ ] ⏳ Create `OTAProgress.jsx`
  - [ ] Device selector for OTA
  - [ ] Progress bar for chunk download
  - [ ] Status indicators (downloading, verifying, installing)
  - [ ] Success/failure notification
- [ ] ⏳ Create `FOTA.jsx` page layout
- [ ] ⏳ Add OTA status polling
- [ ] ⏳ Implement rollback button
- [ ] ⏳ Test OTA workflow end-to-end

### Estimated Time: 5-7 days

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
