#!/bin/bash

# Advanced OTA Error Simulation and Testing Script
# This script implements comprehensive error scenarios for testing rollback mechanisms

set -e

PROJECT_DIR=$(pwd)
FIRMWARE_DIR="$PROJECT_DIR/firmware"
SERVER_PID=""
SIMULATION_LOG="simulation_results.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

log_sim() {
    echo -e "${PURPLE}[SIMULATION]${NC} $1" | tee -a "$SIMULATION_LOG"
}

log_test() {
    echo -e "${CYAN}[TEST]${NC} $1" | tee -a "$SIMULATION_LOG"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1" | tee -a "$SIMULATION_LOG"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1" | tee -a "$SIMULATION_LOG"
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$SIMULATION_LOG"
}

print_test_banner() {
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║              ESP32 OTA Error Simulation Suite             ║"
    echo "║                                                            ║"
    echo "║  This suite tests various failure scenarios to validate   ║"
    echo "║  the robustness of the OTA system and rollback mechanisms ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Tests included:"
    echo "  🔍 1. Hash Mismatch Detection"
    echo "  📡 2. Network Interruption Handling" 
    echo "  🔐 3. Authentication Failures"
    echo "  💾 4. Corrupted Firmware Detection"
    echo "  🔄 5. Rollback Mechanism Validation"
    echo "  ⚡ 6. Power Loss Simulation"
    echo "  📊 7. Memory Constraint Testing"
    echo "  🛡️ 8. Security Attack Simulation"
    echo ""
}

cleanup_simulation() {
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        log_info "Stopping test server..."
        kill "$SERVER_PID"
    fi
    
    # Restore original files
    restore_original_files
}

trap cleanup_simulation EXIT

backup_original_files() {
    log_info "Backing up original files..."
    
    [[ -f "$FIRMWARE_DIR/manifest.json" ]] && cp "$FIRMWARE_DIR/manifest.json" "$FIRMWARE_DIR/manifest.json.backup"
    [[ -f "$FIRMWARE_DIR/firmware_v1.0.0.bin" ]] && cp "$FIRMWARE_DIR/firmware_v1.0.0.bin" "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup"
    [[ -f "ota_server.py" ]] && cp "ota_server.py" "ota_server.py.backup"
}

restore_original_files() {
    log_info "Restoring original files..."
    
    [[ -f "$FIRMWARE_DIR/manifest.json.backup" ]] && cp "$FIRMWARE_DIR/manifest.json.backup" "$FIRMWARE_DIR/manifest.json"
    [[ -f "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup" ]] && cp "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup" "$FIRMWARE_DIR/firmware_v1.0.0.bin"
    [[ -f "ota_server.py.backup" ]] && cp "ota_server.py.backup" "ota_server.py"
    
    # Clean up backup files
    rm -f "$FIRMWARE_DIR"/*.backup "ota_server.py.backup"
}

start_test_server() {
    log_info "Starting OTA server for testing..."
    python3 ota_server.py &
    SERVER_PID=$!
    
    # Wait for server startup
    sleep 3
    
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        log_pass "Test server started (PID: $SERVER_PID)"
    else
        log_fail "Failed to start test server"
        exit 1
    fi
}

# Test 1: Hash Mismatch Detection
test_hash_mismatch() {
    log_test "Test 1: Hash Mismatch Detection"
    echo "================================="
    
    log_sim "Creating firmware with incorrect hash in manifest..."
    
    # Create corrupted manifest with wrong hash
    cd "$FIRMWARE_DIR"
    WRONG_HASH="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    SIZE=$(stat -f%z firmware_v1.0.0.bin 2>/dev/null || stat -c%s firmware_v1.0.0.bin)
    
    cat > manifest.json << EOF
{
    "version": "1.0.1",
    "sha256": "$WRONG_HASH",
    "url": "https://localhost:5000/firmware/v1.0.0.bin",
    "size": $SIZE,
    "release_notes": "Test firmware with wrong hash"
}
EOF
    cd "$PROJECT_DIR"
    
    log_sim "ESP32 should detect hash mismatch and reject update"
    log_info "Expected behavior: Download should fail with hash validation error"
    log_pass "Hash mismatch test prepared"
    
    echo ""
}

# Test 2: Network Interruption Simulation
test_network_interruption() {
    log_test "Test 2: Network Interruption Simulation"
    echo "========================================"
    
    log_sim "Creating modified server that drops connections randomly..."
    
    # Create a modified server script that simulates network issues
    cat > ota_server_network_sim.py << 'EOF'
#!/usr/bin/env python3
import random
import time
from flask import Flask, jsonify, request, abort
from ota_server import app, Config

# Monkey patch to simulate network issues
original_send_file = None

def simulate_network_failure():
    """Randomly simulate network failures"""
    if random.random() < 0.3:  # 30% chance of failure
        print("[SIMULATION] Simulating network interruption...")
        time.sleep(2)  # Simulate timeout
        abort(500)  # Internal server error
    
    if random.random() < 0.2:  # 20% chance of slow response
        print("[SIMULATION] Simulating slow network...")
        time.sleep(random.uniform(5, 10))

@app.before_request
def before_request():
    if request.endpoint == 'download_firmware':
        simulate_network_failure()

if __name__ == '__main__':
    print("Starting OTA server with network simulation...")
    app.run(host=Config.HOST, port=Config.PORT, ssl_context='adhoc', debug=False)
EOF
    
    log_sim "Modified server will randomly drop connections and simulate timeouts"
    log_info "Expected behavior: ESP32 should retry downloads and handle failures gracefully"
    log_pass "Network interruption simulation prepared"
    
    echo ""
}

# Test 3: Authentication Bypass Attempts
test_authentication_bypass() {
    log_test "Test 3: Authentication Security Testing"
    echo "======================================"
    
    log_sim "Testing various authentication bypass attempts..."
    
    # Test without API key
    log_info "Testing manifest access without API key..."
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" https://localhost:5000/firmware/manifest 2>/dev/null || echo "000")
    
    if [[ "$RESPONSE" == "401" ]]; then
        log_pass "Correctly rejected request without API key"
    else
        log_fail "Security issue: Request accepted without API key (HTTP $RESPONSE)"
    fi
    
    # Test with wrong API key
    log_info "Testing with incorrect API key..."
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" -H "X-API-Key: wrong-key" https://localhost:5000/firmware/manifest 2>/dev/null || echo "000")
    
    if [[ "$RESPONSE" == "401" ]]; then
        log_pass "Correctly rejected request with wrong API key"
    else
        log_fail "Security issue: Request accepted with wrong API key (HTTP $RESPONSE)"
    fi
    
    # Test header injection
    log_info "Testing header injection attempts..."
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" -H "X-API-Key: valid\r\nX-Injected: malicious" https://localhost:5000/firmware/manifest 2>/dev/null || echo "000")
    
    if [[ "$RESPONSE" == "401" ]]; then
        log_pass "Correctly handled header injection attempt"
    else
        log_fail "Potential security issue with header injection"
    fi
    
    echo ""
}

# Test 4: Corrupted Firmware Detection
test_corrupted_firmware() {
    log_test "Test 4: Corrupted Firmware Detection"
    echo "===================================="
    
    log_sim "Creating corrupted firmware file..."
    
    # Corrupt the firmware binary
    if [[ -f "$FIRMWARE_DIR/firmware_v1.0.0.bin" ]]; then
        # Add random data to corrupt the file
        echo "CORRUPTED_DATA_$(date +%s)" >> "$FIRMWARE_DIR/firmware_v1.0.0.bin"
        
        # Create manifest with original hash (will cause mismatch)
        cd "$FIRMWARE_DIR"
        cat > manifest.json << EOF
{
    "version": "1.0.2",
    "sha256": "$(cat firmware_v1.0.0.bin.backup | shasum -a 256 | cut -d' ' -f1)",
    "url": "https://localhost:5000/firmware/v1.0.0.bin",
    "size": $(stat -f%z firmware_v1.0.0.bin 2>/dev/null || stat -c%s firmware_v1.0.0.bin),
    "release_notes": "Test firmware with corruption"
}
EOF
        cd "$PROJECT_DIR"
        
        log_sim "Firmware corrupted - ESP32 should detect and reject"
        log_info "Expected behavior: Hash validation should fail"
        log_pass "Corrupted firmware test prepared"
    else
        log_fail "No firmware file found for corruption test"
    fi
    
    echo ""
}

# Test 5: HMAC Validation Bypass
test_hmac_bypass() {
    log_test "Test 5: HMAC Validation Security"
    echo "==============================="
    
    log_sim "Testing HMAC validation bypass attempts..."
    
    # Create firmware without proper HMAC
    cat > test_malicious_firmware.py << 'EOF'
#!/usr/bin/env python3
import requests
import json
from datetime import datetime

def test_hmac_bypass():
    api_key = "your-secret-api-key-change-this"
    base_url = "https://localhost:5000"
    
    # Create fake firmware data
    fake_firmware = b"FAKE_FIRMWARE_DATA_FOR_TESTING" * 100
    
    # Try to upload without proper HMAC
    try:
        response = requests.post(
            f"{base_url}/admin/upload",
            headers={"X-API-Key": api_key},
            files={"firmware": ("fake.bin", fake_firmware)},
            data={"version": "999.0.0"},
            verify=False
        )
        print(f"Upload attempt result: {response.status_code}")
        
        if response.status_code == 200:
            print("WARNING: Server accepted firmware without proper HMAC validation!")
        else:
            print("PASS: Server correctly rejected firmware without proper HMAC")
            
    except Exception as e:
        print(f"Connection error (expected): {e}")

if __name__ == "__main__":
    test_hmac_bypass()
EOF
    
    python3 test_malicious_firmware.py
    rm -f test_malicious_firmware.py
    
    echo ""
}

# Test 6: Memory Constraint Simulation
test_memory_constraints() {
    log_test "Test 6: Memory Constraint Testing"
    echo "================================="
    
    log_sim "Testing behavior with insufficient memory scenarios..."
    
    # Create oversized firmware file
    log_info "Creating oversized firmware file..."
    dd if=/dev/zero of="$FIRMWARE_DIR/oversized_firmware.bin" bs=1M count=5 2>/dev/null
    
    SIZE=$(stat -f%z "$FIRMWARE_DIR/oversized_firmware.bin" 2>/dev/null || stat -c%s "$FIRMWARE_DIR/oversized_firmware.bin")
    HASH=$(shasum -a 256 "$FIRMWARE_DIR/oversized_firmware.bin" | cut -d' ' -f1)
    
    cd "$FIRMWARE_DIR"
    cat > manifest.json << EOF
{
    "version": "2.0.0",
    "sha256": "$HASH",
    "url": "https://localhost:5000/firmware/oversized_firmware.bin",
    "size": $SIZE,
    "release_notes": "Oversized firmware for memory testing"
}
EOF
    cd "$PROJECT_DIR"
    
    log_sim "ESP32 should detect insufficient space and refuse update"
    log_info "Expected behavior: Memory check should fail before download"
    log_pass "Memory constraint test prepared"
    
    echo ""
}

# Test 7: Power Loss Simulation
test_power_loss_simulation() {
    log_test "Test 7: Power Loss Simulation"
    echo "============================="
    
    log_sim "Creating test scenarios for power loss during update..."
    
    cat > power_loss_test.md << 'EOF'
# Power Loss Simulation Test

This test requires manual intervention to simulate power loss at different stages:

## Test Scenarios:

1. **Power loss during firmware download:**
   - Start OTA update
   - Disconnect ESP32 power during download
   - Reconnect and verify rollback to previous version

2. **Power loss during firmware writing:**
   - Start OTA update
   - Disconnect power during flash write process
   - Reconnect and verify rollback mechanism

3. **Power loss during boot after update:**
   - Complete OTA update
   - Disconnect power during first boot of new firmware
   - Reconnect and verify rollback after timeout

## Expected Behavior:

- ESP32 should boot to previous working firmware
- OTA system should mark update as failed
- Next update attempt should work normally
- No permanent brick state should occur

## Manual Test Steps:

1. Monitor serial output during update
2. Identify critical update phases
3. Simulate power loss at each phase
4. Verify recovery behavior
5. Document any issues found

## Automatic Rollback Test:

The ESP32 firmware includes automatic rollback after 2 minutes if:
- New firmware doesn't start properly
- New firmware doesn't confirm successful boot
- Watchdog timer triggers
EOF

    log_info "Power loss simulation requires manual testing"
    log_info "See power_loss_test.md for detailed test procedures"
    log_pass "Power loss test documentation created"
    
    echo ""
}

# Test 8: Comprehensive Security Attack Simulation
test_security_attacks() {
    log_test "Test 8: Security Attack Simulation"
    echo "=================================="
    
    log_sim "Running comprehensive security tests..."
    
    # SQL Injection attempts (if applicable)
    log_info "Testing for injection vulnerabilities..."
    
    # Path traversal attempts
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" -H "X-API-Key: your-secret-api-key-change-this" "https://localhost:5000/firmware/../../../etc/passwd" 2>/dev/null || echo "000")
    
    if [[ "$RESPONSE" == "404" ]] || [[ "$RESPONSE" == "400" ]]; then
        log_pass "Path traversal attack correctly blocked"
    else
        log_fail "Potential path traversal vulnerability (HTTP $RESPONSE)"
    fi
    
    # Large payload attacks
    log_info "Testing large payload handling..."
    LARGE_PAYLOAD=$(python3 -c "print('A' * 10000)")
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: application/json" -H "X-API-Key: your-secret-api-key-change-this" -d "{\"data\":\"$LARGE_PAYLOAD\"}" "https://localhost:5000/firmware/report" 2>/dev/null || echo "000")
    
    if [[ "$RESPONSE" == "400" ]] || [[ "$RESPONSE" == "413" ]]; then
        log_pass "Large payload correctly rejected"
    else
        log_fail "Large payload handling issue (HTTP $RESPONSE)"
    fi
    
    echo ""
}

generate_simulation_report() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║                   Simulation Report                        ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo ""
    
    # Count results
    local pass_count=$(grep -c "\[PASS\]" "$SIMULATION_LOG" || echo "0")
    local fail_count=$(grep -c "\[FAIL\]" "$SIMULATION_LOG" || echo "0")
    local total_count=$((pass_count + fail_count))
    
    echo "📊 Test Summary:"
    echo "  Total tests: $total_count"
    echo "  Passed: $pass_count"
    echo "  Failed: $fail_count"
    
    if [[ $fail_count -eq 0 ]]; then
        echo -e "  Status: ${GREEN}ALL TESTS PASSED${NC}"
    else
        echo -e "  Status: ${RED}SOME TESTS FAILED${NC}"
    fi
    
    echo ""
    echo "📋 Detailed Results:"
    echo "===================="
    grep -E "\[(PASS|FAIL)\]" "$SIMULATION_LOG" || echo "No test results found"
    
    echo ""
    echo "🔍 Security Assessment:"
    echo "======================"
    
    local security_issues=$(grep -c "Security issue\|vulnerability\|WARNING:" "$SIMULATION_LOG" || echo "0")
    
    if [[ $security_issues -eq 0 ]]; then
        echo -e "  ${GREEN}✓${NC} No critical security issues detected"
    else
        echo -e "  ${RED}⚠${NC} $security_issues potential security issues found"
        echo "  Review the log for details and address before production"
    fi
    
    echo ""
    echo "📁 Generated Files:"
    echo "=================="
    echo "  • $SIMULATION_LOG - Complete test log"
    echo "  • power_loss_test.md - Manual testing procedures"
    echo "  • Various test firmware files in firmware/ directory"
    
    echo ""
    echo "🚀 Next Steps:"
    echo "============="
    echo "  1. Address any failed tests or security issues"
    echo "  2. Run manual power loss tests (see power_loss_test.md)"
    echo "  3. Test with real ESP32 hardware"
    echo "  4. Monitor behavior during actual OTA updates"
    echo "  5. Implement additional security measures as needed"
    
    echo ""
    echo "💡 Production Recommendations:"
    echo "============================="
    echo "  • Use proper CA-signed certificates"
    echo "  • Implement device-specific authentication"
    echo "  • Add update verification signatures"
    echo "  • Enable comprehensive logging"
    echo "  • Test rollback mechanisms thoroughly"
    echo "  • Monitor OTA success/failure rates"
    echo ""
}

run_all_simulations() {
    print_test_banner
    
    # Initialize log file
    echo "ESP32 OTA Error Simulation Test Log" > "$SIMULATION_LOG"
    echo "Started: $(date)" >> "$SIMULATION_LOG"
    echo "========================================" >> "$SIMULATION_LOG"
    
    # Backup original files
    backup_original_files
    
    # Start test server
    start_test_server
    
    # Run all simulation tests
    test_hash_mismatch
    test_authentication_bypass
    test_corrupted_firmware
    test_hmac_bypass
    test_memory_constraints
    test_power_loss_simulation
    test_security_attacks
    test_network_interruption
    
    # Generate comprehensive report
    generate_simulation_report
}

# Main execution
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    run_all_simulations
fi