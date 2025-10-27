"""
M4 Security - MAC/HMAC Validation Tests (Flask Server)

Tests:
1. Valid MAC validation
2. Tampered payload detection
3. Tampered MAC detection
4. Invalid MAC format
5. Missing MAC field
6. Missing nonce field
7. Missing payload field
8. Malformed JSON
9. Empty MAC
10. Non-hex MAC characters
11. Wrong MAC length
12. Statistics tracking
13. Device tracking
14. Large payload HMAC
15. Empty payload HMAC
"""

import pytest
import json
import base64
import hmac
import hashlib
import os
import sys
from pathlib import Path
from flask import Flask

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'scripts'))

try:
    from flask_server_modular import app
except ImportError:
    from sys import path
    path.insert(0, '../..')
    from flask_server_modular import app

from handlers.security_handler import (
    validate_secured_payload, 
    get_security_stats, 
    reset_security_stats, 
    clear_nonces,
    PSK_HMAC,
    NONCE_STATE_FILE,
    verify_secured_payload_core,
    SecurityError,
    clear_all_nonces
)
from utils.logger_utils import get_logger

logger = get_logger(__name__)

# Test configuration
TEST_DEVICE_ID = "TEST_MAC_HMAC_FLASK"
# Use a counter to ensure nonces always increase
_nonce_counter = 20000  # Start high to avoid conflicts

def get_next_nonce():
    """Get next sequential nonce"""
    global _nonce_counter
    _nonce_counter += 1
    return _nonce_counter

def create_secured_payload(device_id: str, nonce: int, data: dict) -> dict:
    """
    Create a properly formatted secured payload for testing
    Matches ESP32 security layer format exactly
    
    Args:
        device_id: Device identifier
        nonce: Nonce value (counter, must fit in 4 bytes / uint32)
        data: Actual data to secure
        
    Returns:
        dict: Secured payload with nonce, payload (base64), and MAC
    """
    # Convert data to JSON
    data_json = json.dumps(data)
    payload_bytes = data_json.encode('utf-8')
    
    # Encode payload to base64
    payload_b64 = base64.b64encode(payload_bytes).decode('utf-8')
    
    # Create MAC: HMAC-SHA256(nonce_bytes (4 bytes big-endian) + payload_str)
    nonce_bytes = nonce.to_bytes(4, 'big')
    data_to_sign = nonce_bytes + data_json.encode('utf-8')
    mac = hmac.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
    
    return {
        'nonce': nonce,
        'payload': payload_b64,
        'mac': mac,
        'encrypted': False
    }


@pytest.fixture
def client():
    """Flask test client fixture"""
    global _nonce_counter
    _nonce_counter = 20000  # Reset counter for each test class
    
    app.config['TESTING'] = True
    
    # Clear security state before each test
    reset_security_stats()
    clear_nonces()
    
    # Delete persistent nonce state file if it exists
    if os.path.exists(NONCE_STATE_FILE):
        try:
            os.remove(NONCE_STATE_FILE)
        except:
            pass
    
    # Force clear the last_valid_nonce dict in security_handler
    clear_all_nonces()
    
    with app.app_context():
        with app.test_client() as client:
            yield client
            
    # Clean up after test
    clear_all_nonces()
    if os.path.exists(NONCE_STATE_FILE):
        try:
            os.remove(NONCE_STATE_FILE)
        except:
            pass


@pytest.fixture
def sample_payload():
    """Sample secured payload for testing"""
    nonce = get_next_nonce()
    data = {
        'device_id': TEST_DEVICE_ID,
        'voltage': 5000,
        'current': 500,
        'power': 2000
    }
    return create_secured_payload(TEST_DEVICE_ID, nonce, data)


class TestMACHMACValidation:
    """M4 Security MAC/HMAC Validation Test Suite"""
    
    def setup_method(self):
        """Reset security state before each test"""
        global _nonce_counter
        _nonce_counter = 20000
        
        # Clear all security state
        reset_security_stats()
        clear_nonces()
        
        # Delete persistent nonce state file
        if os.path.exists(NONCE_STATE_FILE):
            try:
                os.remove(NONCE_STATE_FILE)
            except:
                pass
        
        # Force clear nonce tracking
        clear_all_nonces()
    
    def test_valid_mac_validation(self):
        """Test 1: Valid MAC validation"""
        logger.info("=== Test 1: Valid MAC Validation ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Validate using security handler
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is True, f"Validation should succeed but got error: {error}"
        assert decrypted is not None, "Decrypted payload should not be None"
        assert error is None, "Error should be None for valid payload"
        
        # Parse decrypted payload
        decrypted_data = json.loads(decrypted)
        assert decrypted_data['device_id'] == TEST_DEVICE_ID
        assert decrypted_data['voltage'] == 3300
        
        logger.info("[PASS] Valid MAC accepted")
    
    def test_tampered_payload_detection(self):
        """Test 2: Tampered payload detection"""
        logger.info("=== Test 2: Tampered Payload Detection ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Tamper with the payload by decoding, modifying, and re-encoding
        # This ensures valid base64 but different content
        original_b64 = payload['payload']
        decoded = base64.b64decode(original_b64).decode('utf-8')
        # Change voltage value in the JSON
        tampered_data = decoded.replace('3300', '9999')
        tampered_b64 = base64.b64encode(tampered_data.encode('utf-8')).decode('utf-8')
        payload['payload'] = tampered_b64
        
        # Validation should fail (MAC won't match tampered payload)
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for tampered payload"
        assert error is not None, "Error message should be provided"
        assert 'hmac' in error.lower() or 'mac' in error.lower(), f"Error should mention MAC: {error}"
        
        logger.info("[PASS] Tampered payload detected")
    
    def test_tampered_mac_detection(self):
        """Test 3: Tampered MAC detection"""
        logger.info("=== Test 3: Tampered MAC Detection ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Tamper with the MAC
        original_mac = payload['mac']
        tampered_mac = 'a' * len(original_mac)  # Replace with all 'a's
        payload['mac'] = tampered_mac
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for tampered MAC"
        assert error is not None, "Error message should be provided"
        assert 'hmac' in error.lower() or 'mac' in error.lower(), f"Error should mention MAC: {error}"
        
        logger.info("[PASS] Tampered MAC detected")
    
    def test_invalid_mac_format(self):
        """Test 4: Invalid MAC format - uppercase hex should fail"""
        logger.info("=== Test 4: Invalid MAC Format ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Use uppercase hex (HMAC in Python uses lowercase, so this will fail verification)
        payload['mac'] = payload['mac'].upper()
        
        # Validation should fail (MAC comparison is case-sensitive)
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail with uppercase hex MAC (case-sensitive)"
        assert error is not None, "Error message should be provided"
        assert 'hmac' in error.lower() or 'mac' in error.lower(), f"Error should mention MAC: {error}"
        
        logger.info("[PASS] Invalid MAC format (uppercase) detected")
    
    def test_missing_mac_field(self):
        """Test 5: Missing MAC field"""
        logger.info("=== Test 5: Missing MAC Field ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Remove MAC field
        del payload['mac']
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for missing MAC"
        assert error is not None, "Error message should be provided"
        assert 'mac' in error.lower() or 'field' in error.lower(), f"Error should mention missing field: {error}"
        
        logger.info("[PASS] Missing MAC field detected")
    
    def test_missing_nonce_field(self):
        """Test 6: Missing nonce field"""
        logger.info("=== Test 6: Missing Nonce Field ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Remove nonce field
        del payload['nonce']
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for missing nonce"
        assert error is not None, "Error message should be provided"
        assert 'nonce' in error.lower() or 'field' in error.lower(), f"Error should mention missing field: {error}"
        
        logger.info("[PASS] Missing nonce field detected")
    
    def test_missing_payload_field(self):
        """Test 7: Missing payload field"""
        logger.info("=== Test 7: Missing Payload Field ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Remove payload field
        del payload['payload']
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for missing payload"
        assert error is not None, "Error message should be provided"
        assert 'payload' in error.lower() or 'field' in error.lower(), f"Error should mention missing field: {error}"
        
        logger.info("[PASS] Missing payload field detected")
    
    def test_malformed_json(self):
        """Test 8: Malformed JSON"""
        logger.info("=== Test 8: Malformed JSON ===")
        
        # Create malformed JSON
        malformed_json = '{"nonce": 8000, "payload": "invalid", "mac":'  # Incomplete JSON
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, malformed_json)
        
        assert success is False, "Validation should fail for malformed JSON"
        assert error is not None, "Error message should be provided"
        assert 'json' in error.lower() or 'invalid' in error.lower(), f"Error should mention JSON issue: {error}"
        
        logger.info("[PASS] Malformed JSON detected")
    
    def test_empty_mac(self):
        """Test 9: Empty MAC"""
        logger.info("=== Test 9: Empty MAC ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Set empty MAC
        payload['mac'] = ''
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for empty MAC"
        assert error is not None, "Error message should be provided"
        
        logger.info("[PASS] Empty MAC detected")
    
    def test_non_hex_mac(self):
        """Test 10: Non-hex MAC characters"""
        logger.info("=== Test 10: Non-hex MAC Characters ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Use non-hex characters
        payload['mac'] = 'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz'  # 64 chars but not hex
        
        # Validation should fail (MAC won't match)
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for non-hex MAC"
        assert error is not None, "Error message should be provided"
        
        logger.info("[PASS] Non-hex MAC detected")
    
    def test_wrong_mac_length(self):
        """Test 11: Wrong MAC length"""
        logger.info("=== Test 11: Wrong MAC Length ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Use wrong length MAC (32 chars instead of 64)
        payload['mac'] = 'a' * 32
        
        # Validation should fail
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is False, "Validation should fail for wrong MAC length"
        assert error is not None, "Error message should be provided"
        
        logger.info("[PASS] Wrong MAC length detected")
    
    def test_mac_consistency(self):
        """Test 12: MAC consistency - same payload should produce same MAC"""
        logger.info("=== Test 12: MAC Consistency ===")
        
        nonce = get_next_nonce()
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300, 'current': 500}
        
        # Create same payload twice
        payload1 = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        payload2 = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # MACs should be identical
        assert payload1['mac'] == payload2['mac'], "Same payload+nonce should produce same MAC"
        
        # Verify first payload works
        success1, _, _ = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload1))
        assert success1 is True, "First payload should validate"
        
        logger.info("[PASS] MAC consistency verified")
    
    def test_mac_uniqueness(self):
        """Test 13: MAC uniqueness - different payloads produce different MACs"""
        logger.info("=== Test 13: MAC Uniqueness ===")
        
        nonce1 = get_next_nonce()
        nonce2 = get_next_nonce()
        data1 = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        data2 = {'device_id': TEST_DEVICE_ID, 'voltage': 5000}
        
        payload1 = create_secured_payload(TEST_DEVICE_ID, nonce1, data1)
        payload2 = create_secured_payload(TEST_DEVICE_ID, nonce2, data2)
        
        # MACs should be different
        assert payload1['mac'] != payload2['mac'], "Different payloads should produce different MACs"
        
        logger.info("[PASS] MAC uniqueness verified")
    
    def test_large_payload_hmac(self):
        """Test 14: Large payload HMAC"""
        logger.info("=== Test 14: Large Payload HMAC ===")
        
        nonce = get_next_nonce()
        # Create large payload with 20 sensor readings
        data = {
            'device_id': TEST_DEVICE_ID,
            'readings': [
                {'v': 3300 + i, 'c': 500 + i, 'p': 1000 + i, 't': 25 + i} 
                for i in range(20)
            ]
        }
        
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Validation should succeed
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is True, f"Large payload validation should succeed: {error}"
        assert decrypted is not None, "Decrypted payload should not be None"
        
        # Verify data integrity
        decrypted_data = json.loads(decrypted)
        assert len(decrypted_data['readings']) == 20, "All 20 readings should be present"
        
        logger.info("[PASS] Large payload HMAC validated")
    
    def test_empty_payload_hmac(self):
        """Test 15: Empty payload HMAC"""
        logger.info("=== Test 15: Empty Payload HMAC ===")
        
        nonce = get_next_nonce()
        data = {}  # Empty payload
        
        payload = create_secured_payload(TEST_DEVICE_ID, nonce, data)
        
        # Validation should succeed (empty payload is valid)
        success, decrypted, error = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload))
        
        assert success is True, f"Empty payload validation should succeed: {error}"
        assert decrypted is not None, "Decrypted payload should not be None"
        
        # Verify empty data
        decrypted_data = json.loads(decrypted)
        assert decrypted_data == {}, "Decrypted data should be empty dict"
        
        logger.info("[PASS] Empty payload HMAC validated")
    
    def test_nonce_sensitivity(self):
        """Test 16: Different nonces produce different MACs"""
        logger.info("=== Test 16: Nonce Sensitivity ===")
        
        data = {'device_id': TEST_DEVICE_ID, 'voltage': 3300}
        
        nonce1 = get_next_nonce()
        nonce2 = get_next_nonce()
        
        payload1 = create_secured_payload(TEST_DEVICE_ID, nonce1, data)
        payload2 = create_secured_payload(TEST_DEVICE_ID, nonce2, data)
        
        # Same payload, different nonces should have different MACs
        assert payload1['mac'] != payload2['mac'], "Different nonces should produce different MACs"
        
        # Both should validate
        success1, _, _ = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload1))
        success2, _, _ = validate_secured_payload(TEST_DEVICE_ID, json.dumps(payload2))
        
        assert success1 is True, "First payload should validate"
        assert success2 is True, "Second payload should validate"
        
        logger.info("[PASS] Nonce sensitivity verified")


if __name__ == '__main__':
    pytest.main([__file__, '-v', '--tb=short'])
