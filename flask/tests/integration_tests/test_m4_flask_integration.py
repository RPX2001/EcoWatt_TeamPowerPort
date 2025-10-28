"""
M4 Flask Integration Tests - Real-World End-to-End Testing

Tests the complete M4 workflow from Flask server perspective:
1. Security validation (HMAC verification, anti-replay)
2. Command execution with security
3. Remote configuration updates
4. FOTA (Firmware Over-The-Air) workflow
5. Coordination with ESP32 integration tests

This test suite runs alongside the ESP32 M4 integration test (test_m4_integration/test_main.cpp)
for complete end-to-end validation.

Run with:
    pytest tests/integration_tests/test_m4_integration/test_m4_flask_integration.py -v
    
Or run Flask server in integration mode:
    python tests/integration_tests/test_m4_integration/run_integration_server.py
"""

import pytest
import json
import time
import hmac
import hashlib
import base64
import sys
from pathlib import Path
from flask import Flask

# Add flask directory to path (tests/integration_tests -> flask/)
flask_dir = Path(__file__).parent.parent.parent
sys.path.insert(0, str(flask_dir))

from flask_server_modular import app

from handlers.security_handler import (
    validate_secured_payload, 
    get_security_stats, 
    reset_security_stats, 
    clear_nonces,
    PSK_HMAC
)
from handlers.command_handler import queue_command, poll_commands, get_command_status
from handlers.ota_handler import check_for_update
from utils.logger_utils import get_logger

logger = get_logger(__name__)

# Test configuration
TEST_DEVICE_ID = "ESP32_EcoWatt_Smart"
TEST_FIRMWARE_VERSION = "1.0.4"
PSK_KEY = "EcoWattSecureKey2024"


@pytest.fixture
def client():
    """Flask test client fixture"""
    app.config['TESTING'] = True
    with app.app_context():
        # Reset security state before each test
        reset_security_stats()
        clear_nonces()
        with app.test_client() as client:
            yield client


@pytest.fixture
def nonce_counter():
    """Nonce counter for generating sequential nonces"""
    return {'value': int(time.time() * 1000)}


def generate_nonce(counter):
    """Generate a new nonce"""
    counter['value'] += 1
    return str(counter['value'])


def calculate_hmac(message, key):
    """Calculate HMAC-SHA256"""
    return hmac.new(key.encode(), message.encode(), hashlib.sha256).hexdigest()


def create_secured_payload(payload_data, device_id, nonce_counter, key=PSK_KEY):
    """Create a secured payload with HMAC"""
    payload_str = json.dumps(payload_data)
    nonce = generate_nonce(nonce_counter)
    
    # Calculate HMAC: payload + nonce + device_id
    message = payload_str + nonce + device_id
    mac = calculate_hmac(message, key)
    
    return {
        'payload': payload_str,
        'nonce': nonce,
        'mac': mac,
        'device_id': device_id
    }


class TestM4FlaskIntegration:
    """M4 Flask Integration Test Suite"""
    
    # ========================================================================
    # TEST CATEGORY 1: Server Health & Setup
    # ========================================================================
    
    def test_01_flask_server_health(self, client):
        """Test 1: Flask server is running and healthy"""
        logger.info("=== Test 1: Flask Server Health ===")
        
        response = client.get('/health')
        
        assert response.status_code == 200
        data = json.loads(response.data)
        
        assert 'status' in data
        assert data['status'] == 'healthy'
        
        logger.info("[PASS] Flask server is healthy")
    
    def test_02_security_layer_initialized(self, client):
        """Test 2: Security layer is properly initialized"""
        logger.info("=== Test 2: Security Layer Initialization ===")
        
        stats = get_security_stats()
        
        assert 'total_validations' in stats
        assert 'successful_validations' in stats
        assert 'failed_validations' in stats
        
        logger.info("[PASS] Security layer initialized correctly")
    
    # ========================================================================
    # TEST CATEGORY 2: Security - HMAC Verification
    # ========================================================================
    
    def test_03_secured_upload_valid_hmac(self, client, nonce_counter):
        """Test 3: Accept upload with valid HMAC"""
        logger.info("=== Test 3: Valid HMAC Upload ===")
        
        # Create test data
        data = {
            'current': 2.5,
            'voltage': 230.0,
            'power': 575.0,
            'timestamp': int(time.time() * 1000)
        }
        
        # Secure the payload
        secured = create_secured_payload(data, TEST_DEVICE_ID, nonce_counter)
        
        # Send to server
        response = client.post('/upload',
                              json=secured,
                              content_type='application/json')
        
        assert response.status_code == 200
        response_data = json.loads(response.data)
        
        assert response_data.get('status') == 'success'
        
        logger.info("[PASS] Valid HMAC accepted")
    
    def test_04_secured_upload_invalid_hmac(self, client, nonce_counter):
        """Test 4: Reject upload with invalid HMAC"""
        logger.info("=== Test 4: Invalid HMAC Rejection ===")
        
        # Create payload with invalid HMAC
        nonce = generate_nonce(nonce_counter)
        secured = {
            'payload': '{"current":2.5}',
            'nonce': nonce,
            'mac': 'invalid_hmac_123456789abcdef',
            'device_id': TEST_DEVICE_ID
        }
        
        response = client.post('/upload',
                              json=secured,
                              content_type='application/json')
        
        assert response.status_code in [400, 401]
        
        logger.info("[PASS] Invalid HMAC rejected correctly")
    
    def test_05_secured_upload_tampered_payload(self, client, nonce_counter):
        """Test 5: Detect tampered payload"""
        logger.info("=== Test 5: Tampered Payload Detection ===")
        
        # Create valid secured payload
        data = {'current': 2.5}
        secured = create_secured_payload(data, TEST_DEVICE_ID, nonce_counter)
        
        # Tamper with the payload (but keep original HMAC)
        tampered_data = {'current': 10.0}  # Different value
        secured['payload'] = json.dumps(tampered_data)
        
        response = client.post('/upload',
                              json=secured,
                              content_type='application/json')
        
        assert response.status_code in [400, 401]
        
        logger.info("[PASS] Tampered payload detected")
    
    # ========================================================================
    # TEST CATEGORY 3: Security - Anti-Replay Protection
    # ========================================================================
    
    def test_06_anti_replay_duplicate_nonce(self, client, nonce_counter):
        """Test 6: Reject duplicate nonce (replay attack)"""
        logger.info("=== Test 6: Anti-Replay - Duplicate Nonce ===")
        
        # Create secured payload
        data = {'current': 2.5}
        secured = create_secured_payload(data, TEST_DEVICE_ID, nonce_counter)
        
        # First request should succeed
        response1 = client.post('/upload',
                               json=secured,
                               content_type='application/json')
        
        assert response1.status_code == 200
        
        # Second request with same nonce should fail
        response2 = client.post('/upload',
                               json=secured,
                               content_type='application/json')
        
        assert response2.status_code in [400, 401]
        
        logger.info("[PASS] Duplicate nonce rejected (replay attack blocked)")
    
    def test_07_anti_replay_old_nonce(self, client, nonce_counter):
        """Test 7: Reject old nonce"""
        logger.info("=== Test 7: Anti-Replay - Old Nonce ===")
        
        # Send several new nonces to move window forward
        for _ in range(5):
            data = {'current': 2.5}
            secured = create_secured_payload(data, TEST_DEVICE_ID, nonce_counter)
            client.post('/upload', json=secured, content_type='application/json')
        
        # Try to use an old nonce value
        old_nonce = str(nonce_counter['value'] - 1000)
        message = json.dumps({'current': 2.5}) + old_nonce + TEST_DEVICE_ID
        mac = calculate_hmac(message, PSK_KEY)
        
        old_secured = {
            'payload': '{"current":2.5}',
            'nonce': old_nonce,
            'mac': mac,
            'device_id': TEST_DEVICE_ID
        }
        
        response = client.post('/upload',
                              json=old_secured,
                              content_type='application/json')
        
        assert response.status_code in [400, 401]
        
        logger.info("[PASS] Old nonce rejected")
    
    # ========================================================================
    # TEST CATEGORY 4: Command Execution
    # ========================================================================
    
    def test_08_command_queue_set_power(self, client):
        """Test 8: Queue set_power command"""
        logger.info("=== Test 8: Command Queue - Set Power ===")
        
        command_payload = {
            'device_id': TEST_DEVICE_ID,
            'command_type': 'set_power',
            'parameters': {
                'power_value': 5000
            }
        }
        
        response = client.post('/command/queue',
                              json=command_payload,
                              content_type='application/json')
        
        assert response.status_code == 200
        data = json.loads(response.data)
        
        assert 'command_id' in data
        assert data.get('status') == 'queued'
        
        logger.info(f"[PASS] Command queued with ID: {data['command_id']}")
    
    def test_09_command_poll_pending(self, client):
        """Test 9: Poll for pending commands"""
        logger.info("=== Test 9: Command Poll - Pending ===")
        
        # Queue a command first
        command_payload = {
            'device_id': TEST_DEVICE_ID,
            'command_type': 'write_register',
            'parameters': {
                'address': 40001,
                'value': 4500
            }
        }
        
        client.post('/command/queue', json=command_payload, content_type='application/json')
        
        # Poll for commands
        response = client.get(f'/commands/pending?device_id={TEST_DEVICE_ID}')
        
        assert response.status_code == 200
        data = json.loads(response.data)
        
        assert 'commands' in data
        assert len(data['commands']) > 0
        
        logger.info(f"[PASS] Found {len(data['commands'])} pending command(s)")
    
    def test_10_command_execution_result(self, client):
        """Test 10: Report command execution result"""
        logger.info("=== Test 10: Command Execution Result ===")
        
        # Queue a command
        command_payload = {
            'device_id': TEST_DEVICE_ID,
            'command_type': 'set_power',
            'parameters': {'power_value': 3000}
        }
        
        queue_response = client.post('/command/queue',
                                    json=command_payload,
                                    content_type='application/json')
        
        command_id = json.loads(queue_response.data)['command_id']
        
        # Report execution result
        result_payload = {
            'command_id': command_id,
            'status': 'completed',
            'result': {
                'success': True,
                'message': 'Power set to 3000W (30%)'
            }
        }
        
        response = client.post('/command/result',
                              json=result_payload,
                              content_type='application/json')
        
        assert response.status_code == 200
        
        logger.info("[PASS] Command result reported successfully")
    
    # ========================================================================
    # TEST CATEGORY 5: Remote Configuration
    # ========================================================================
    
    def test_11_config_update_valid(self, client):
        """Test 11: Update device configuration"""
        logger.info("=== Test 11: Remote Configuration Update ===")
        
        config_payload = {
            'device_id': TEST_DEVICE_ID,
            'config': {
                'sample_rate_hz': 10,
                'upload_interval_s': 60,
                'compression_enabled': True,
                'power_threshold_w': 1000
            }
        }
        
        response = client.post('/config/update',
                              json=config_payload,
                              content_type='application/json')
        
        assert response.status_code == 200
        data = json.loads(response.data)
        
        assert data.get('status') == 'updated'
        
        logger.info("[PASS] Configuration updated successfully")
    
    def test_12_config_retrieve(self, client):
        """Test 12: Retrieve device configuration"""
        logger.info("=== Test 12: Configuration Retrieval ===")
        
        response = client.get(f'/config?device_id={TEST_DEVICE_ID}')
        
        assert response.status_code == 200
        data = json.loads(response.data)
        
        assert 'config' in data
        assert 'device_id' in data
        
        logger.info("[PASS] Configuration retrieved successfully")
    
    # ========================================================================
    # TEST CATEGORY 6: FOTA (Firmware Over-The-Air)
    # ========================================================================
    
    def test_13_fota_check_update(self, client):
        """Test 13: Check for firmware update"""
        logger.info("=== Test 13: FOTA - Check Update ===")
        
        response = client.get(f'/ota/check?device_id={TEST_DEVICE_ID}&version={TEST_FIRMWARE_VERSION}')
        
        assert response.status_code == 200
        data = json.loads(response.data)
        
        assert 'update_available' in data
        
        if data['update_available']:
            assert 'version' in data
            assert 'download_url' in data
            logger.info(f"[PASS] Update available: {data['version']}")
        else:
            logger.info("[PASS] No update available (current version up to date)")
    
    def test_14_fota_firmware_info(self, client):
        """Test 14: Get firmware information"""
        logger.info("=== Test 14: FOTA - Firmware Info ===")
        
        response = client.get(f'/ota/info?version=1.0.5')
        
        # May return 200 with info or 404 if version doesn't exist
        assert response.status_code in [200, 404]
        
        if response.status_code == 200:
            data = json.loads(response.data)
            assert 'version' in data
            assert 'size' in data
            logger.info(f"[PASS] Firmware info retrieved: v{data['version']}, {data['size']} bytes")
        else:
            logger.info("[PASS] Firmware version not found (expected for non-existent version)")
    
    @pytest.mark.skip(reason="Requires actual firmware binary")
    def test_15_fota_download_firmware(self, client):
        """Test 15: Download firmware binary"""
        logger.info("=== Test 15: FOTA - Download Firmware ===")
        
        response = client.get(f'/ota/download?device_id={TEST_DEVICE_ID}&version=1.0.5')
        
        assert response.status_code == 200
        assert response.content_type == 'application/octet-stream'
        assert len(response.data) > 0
        
        logger.info(f"[PASS] Firmware downloaded: {len(response.data)} bytes")
    
    # ========================================================================
    # TEST CATEGORY 7: Combined Workflows
    # ========================================================================
    
    def test_16_workflow_secured_upload_with_command(self, client, nonce_counter):
        """Test 16: Secured upload followed by command execution"""
        logger.info("=== Test 16: Workflow - Upload + Command ===")
        
        # Step 1: Secured upload
        data = {'current': 3.0, 'voltage': 235.0, 'power': 705.0}
        secured = create_secured_payload(data, TEST_DEVICE_ID, nonce_counter)
        
        upload_response = client.post('/upload',
                                     json=secured,
                                     content_type='application/json')
        
        assert upload_response.status_code == 200
        
        # Step 2: Queue command
        command_payload = {
            'device_id': TEST_DEVICE_ID,
            'command_type': 'set_power',
            'parameters': {'power_value': 4000}
        }
        
        command_response = client.post('/command/queue',
                                      json=command_payload,
                                      content_type='application/json')
        
        assert command_response.status_code == 200
        
        # Step 3: Poll for command
        poll_response = client.get(f'/commands/pending?device_id={TEST_DEVICE_ID}')
        
        assert poll_response.status_code == 200
        poll_data = json.loads(poll_response.data)
        
        assert len(poll_data['commands']) > 0
        
        logger.info("[PASS] Complete workflow: upload → queue → poll successful")
    
    def test_17_workflow_config_then_fota(self, client):
        """Test 17: Config update followed by FOTA check"""
        logger.info("=== Test 17: Workflow - Config + FOTA ===")
        
        # Step 1: Update config
        config_payload = {
            'device_id': TEST_DEVICE_ID,
            'config': {'upload_interval_s': 30}
        }
        
        config_response = client.post('/config/update',
                                     json=config_payload,
                                     content_type='application/json')
        
        assert config_response.status_code == 200
        
        # Step 2: Check for FOTA
        fota_response = client.get(f'/ota/check?device_id={TEST_DEVICE_ID}&version={TEST_FIRMWARE_VERSION}')
        
        assert fota_response.status_code == 200
        
        logger.info("[PASS] Complete workflow: config update → FOTA check successful")
    
    # ========================================================================
    # TEST CATEGORY 8: Statistics and Monitoring
    # ========================================================================
    
    def test_18_security_statistics(self, client):
        """Test 18: Security statistics tracking"""
        logger.info("=== Test 18: Security Statistics ===")
        
        stats = get_security_stats()
        
        assert 'total_validations' in stats
        assert 'successful_validations' in stats
        assert 'failed_validations' in stats
        assert 'replay_attacks_blocked' in stats
        
        # Should have some validations from previous tests
        assert stats['total_validations'] > 0
        
        logger.info(f"[PASS] Security stats: {stats['total_validations']} validations, "
                   f"{stats['successful_validations']} successful, "
                   f"{stats['failed_validations']} failed")


# ============================================================================
# STANDALONE INTEGRATION SERVER (for running with ESP32)
# ============================================================================

def run_integration_server(host='0.0.0.0', port=5001):
    """
    Run Flask server in integration test mode.
    
    This allows the ESP32 integration test to connect and run real tests.
    """
    print("\n" + "="*70)
    print(" " * 15 + "M4 INTEGRATION TEST - FLASK SERVER")
    print("="*70)
    print(f"Server: http://{host}:{port}")
    print(f"Device ID: {TEST_DEVICE_ID}")
    print(f"Firmware Version: {TEST_FIRMWARE_VERSION}")
    print("="*70)
    print("\nTest Categories:")
    print("  1. Security - HMAC verification & anti-replay")
    print("  2. Command Execution - Set power, write registers")
    print("  3. Remote Configuration - Update device config")
    print("  4. FOTA - Firmware download & update")
    print("\nWaiting for ESP32 to connect...")
    print("="*70 + "\n")
    
    try:
        app.run(host=host, port=port, debug=False)
    except KeyboardInterrupt:
        print("\n\nIntegration server stopped")


if __name__ == '__main__':
    # Run as standalone integration server
    run_integration_server()
