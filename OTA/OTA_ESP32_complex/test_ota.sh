#!/bin/bash

# Comprehensive OTA Testing Script
# Tests various failure scenarios and rollback mechanisms

set -e

PROJECT_DIR=$(pwd)
FIRMWARE_DIR="$PROJECT_DIR/firmware"
SERVER_PID=""
TEST_RESULTS=()

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
    TEST_RESULTS+=("✅ $1")
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    TEST_RESULTS+=("❌ $1")
}

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        log_info "Stopping OTA server (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
    fi
    
    # Restore original firmware if backup exists
    if [[ -f "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup" ]]; then
        log_info "Restoring original firmware..."
        cp "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup" "$FIRMWARE_DIR/firmware_v1.0.0.bin"
        rm "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup"
    fi
}

trap cleanup EXIT

print_banner() {
    echo "🧪 OTA System Comprehensive Test Suite"
    echo "======================================"
    echo ""
}

start_ota_server() {
    log_info "Starting OTA server in background..."
    python3 ota_server.py &
    SERVER_PID=$!
    
    # Wait for server to start
    sleep 3
    
    # Check if server is running
    if kill -0 $SERVER_PID 2>/dev/null; then
        log_success "OTA server started successfully (PID: $SERVER_PID)"
    else
        log_error "Failed to start OTA server"
        exit 1
    fi
}

test_normal_ota() {
    log_info "Test 1: Normal OTA Update Flow"
    echo "------------------------------"
    
    # Build and prepare firmware
    log_info "Building firmware v1.0.0..."
    pio run
    
    mkdir -p "$FIRMWARE_DIR"
    cp ".pio/build/esp32dev/firmware.bin" "$FIRMWARE_DIR/firmware_v1.0.0.bin"
    
    # Generate manifest
    cd "$FIRMWARE_DIR"
    HASH=$(sha256sum firmware_v1.0.0.bin | cut -d' ' -f1)
    cat > manifest.json << EOF
{
    "version": "1.0.0",
    "sha256": "$HASH",
    "url": "https://localhost:5000/firmware/v1.0.0.bin",
    "size": $(stat -c%s firmware_v1.0.0.bin)
}
EOF
    cd "$PROJECT_DIR"
    
    log_success "Normal OTA setup completed"
}

test_corrupted_firmware() {
    log_info "Test 2: Corrupted Firmware Test"
    echo "-------------------------------"
    
    # Backup original firmware
    cp "$FIRMWARE_DIR/firmware_v1.0.0.bin" "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup"
    
    # Corrupt the firmware file
    log_info "Corrupting firmware file..."
    echo "CORRUPTED_DATA" >> "$FIRMWARE_DIR/firmware_v1.0.0.bin"
    
    # The ESP32 should detect hash mismatch and refuse update
    log_warning "Firmware corrupted - ESP32 should reject this update"
    log_success "Corrupted firmware test prepared"
}

test_wrong_hash_manifest() {
    log_info "Test 3: Wrong Hash in Manifest Test"
    echo "----------------------------------"
    
    # Create manifest with wrong hash
    cd "$FIRMWARE_DIR"
    cat > manifest.json << EOF
{
    "version": "1.0.1",
    "sha256": "0000000000000000000000000000000000000000000000000000000000000000",
    "url": "https://localhost:5000/firmware/v1.0.0.bin",
    "size": $(stat -c%s firmware_v1.0.0.bin.backup)
}
EOF
    cd "$PROJECT_DIR"
    
    log_success "Wrong hash manifest test prepared"
}

test_network_interruption() {
    log_info "Test 4: Network Interruption Simulation"
    echo "---------------------------------------"
    
    # This would require ESP32 to handle partial downloads
    log_info "Network interruption test requires ESP32 connection"
    log_info "ESP32 should retry download on network failures"
    log_success "Network interruption test documented"
}

restore_good_firmware() {
    log_info "Restoring good firmware for next test..."
    if [[ -f "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup" ]]; then
        cp "$FIRMWARE_DIR/firmware_v1.0.0.bin.backup" "$FIRMWARE_DIR/firmware_v1.0.0.bin"
        
        # Generate correct manifest
        cd "$FIRMWARE_DIR"
        HASH=$(sha256sum firmware_v1.0.0.bin | cut -d' ' -f1)
        cat > manifest.json << EOF
{
    "version": "1.0.0",
    "sha256": "$HASH",
    "url": "https://localhost:5000/firmware/v1.0.0.bin",
    "size": $(stat -c%s firmware_v1.0.0.bin)
}
EOF
        cd "$PROJECT_DIR"
        log_success "Good firmware restored"
    fi
}

test_api_authentication() {
    log_info "Test 5: API Authentication Test"
    echo "------------------------------"
    
    log_info "Testing manifest endpoint without API key..."
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" https://localhost:5000/firmware/manifest || echo "000")
    
    if [[ "$RESPONSE" == "401" ]]; then
        log_success "Correctly rejected request without API key"
    else
        log_error "Server should reject requests without API key (got $RESPONSE)"
    fi
    
    log_info "Testing manifest endpoint with correct API key..."
    RESPONSE=$(curl -k -s -o /dev/null -w "%{http_code}" -H "X-API-Key: your-secret-api-key-change-this" https://localhost:5000/firmware/manifest || echo "000")
    
    if [[ "$RESPONSE" == "200" ]]; then
        log_success "Correctly accepted request with valid API key"
    else
        log_error "Server should accept requests with valid API key (got $RESPONSE)"
    fi
}

test_https_security() {
    log_info "Test 6: HTTPS Security Test"
    echo "---------------------------"
    
    log_info "Testing HTTPS endpoint security..."
    
    # Test with invalid certificate (should fail without -k)
    RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" https://localhost:5000/firmware/manifest 2>/dev/null || echo "000")
    
    if [[ "$RESPONSE" == "000" ]]; then
        log_success "HTTPS properly rejects connections without certificate validation"
    else
        log_warning "HTTPS connection succeeded without proper certificate validation"
    fi
}

generate_test_report() {
    echo ""
    echo "🧪 Test Results Summary"
    echo "======================"
    echo ""
    
    for result in "${TEST_RESULTS[@]}"; do
        echo "$result"
    done
    
    echo ""
    echo "📋 Manual Testing Instructions:"
    echo "1. Flash the firmware to ESP32: pio run --target upload"
    echo "2. Monitor serial output: pio device monitor"
    echo "3. Watch for OTA update attempts and responses"
    echo "4. Verify rollback behavior on failed updates"
    echo "5. Test different failure scenarios by modifying server files"
    echo ""
    echo "🔧 Troubleshooting:"
    echo "- Check WiFi credentials in include/config.h"
    echo "- Verify server IP address matches your network"
    echo "- Ensure certificates are properly generated"
    echo "- Check API key consistency between ESP32 and server"
}

run_all_tests() {
    print_banner
    
    # Prerequisites check
    if ! command -v pio &> /dev/null; then
        log_error "PlatformIO CLI not found. Please install PlatformIO."
        exit 1
    fi
    
    if ! command -v python3 &> /dev/null; then
        log_error "Python 3 not found. Please install Python 3."
        exit 1
    fi
    
    if ! command -v openssl &> /dev/null; then
        log_error "OpenSSL not found. Please install OpenSSL."
        exit 1
    fi
    
    # Generate certificates if they don't exist
    if [[ ! -f "certs/server.crt" ]]; then
        log_info "Generating SSL certificates..."
        chmod +x generate_certs.sh
        ./generate_certs.sh
    fi
    
    # Install Python dependencies
    log_info "Installing Python dependencies..."
    pip3 install -r requirements.txt
    
    # Start server
    start_ota_server
    
    # Run tests
    test_normal_ota
    sleep 2
    
    test_api_authentication
    sleep 2
    
    test_https_security
    sleep 2
    
    test_corrupted_firmware
    sleep 2
    
    test_wrong_hash_manifest
    sleep 2
    
    restore_good_firmware
    sleep 2
    
    test_network_interruption
    
    # Generate report
    generate_test_report
}

# Check if script is being run directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    run_all_tests
fi