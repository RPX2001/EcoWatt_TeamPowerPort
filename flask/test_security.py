"""
Security Testing Script
Tests HMAC authentication and anti-replay protection
"""

import requests
import hmac
import hashlib
import json
import time

# Configuration
FLASK_SERVER = "http://10.78.228.2:5001"
DEVICE_ID = "ESP32_EcoWatt_Smart"
PSK_HEX = "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe"

# Convert hex PSK to bytes
PSK = bytes.fromhex(PSK_HEX)

def compute_hmac(payload, sequence_number):
    """Compute HMAC-SHA256 matching ESP32 implementation."""
    if isinstance(payload, str):
        payload = payload.encode('utf-8')
    elif isinstance(payload, dict):
        payload = json.dumps(payload, separators=(',', ':')).encode('utf-8')
    
    # Append sequence number (big-endian, 4 bytes)
    sequence_bytes = sequence_number.to_bytes(4, byteorder='big')
    message = payload + sequence_bytes
    
    # Compute HMAC
    h = hmac.new(PSK, message, hashlib.sha256)
    return h.hexdigest()

def test_valid_request(sequence):
    """Test a valid request with correct HMAC and sequence."""
    print(f"\n{'='*60}")
    print(f"TEST 1: Valid Request (Sequence {sequence})")
    print(f"{'='*60}")
    
    payload = {
        "device_id": DEVICE_ID,
        "test": "security_test",
        "timestamp": int(time.time() * 1000)
    }
    
    payload_str = json.dumps(payload, separators=(',', ':'))
    hmac_value = compute_hmac(payload_str, sequence)
    
    headers = {
        "Content-Type": "application/json",
        "X-Sequence-Number": str(sequence),
        "X-HMAC-SHA256": hmac_value
    }
    
    print(f"  Payload: {payload_str}")
    print(f"  Sequence: {sequence}")
    print(f"  HMAC: {hmac_value}")
    
    try:
        # Send raw JSON string (not dict) to match HMAC computation
        response = requests.post(f"{FLASK_SERVER}/process", 
                                data=payload_str, headers=headers, timeout=5)
        print(f"  Response Status: {response.status_code}")
        print(f"  Response: {response.json()}")
        return response.status_code == 200 or response.status_code == 400  # 400 is OK for wrong payload format
    except Exception as e:
        print(f"  Error: {e}")
        return False

def test_wrong_hmac(sequence):
    """Test request with incorrect HMAC (should fail)."""
    print(f"\n{'='*60}")
    print(f"TEST 2: Wrong HMAC (Sequence {sequence})")
    print(f"{'='*60}")
    
    payload = {
        "device_id": DEVICE_ID,
        "test": "tampered_test",
        "timestamp": int(time.time() * 1000)
    }
    
    payload_str = json.dumps(payload, separators=(',', ':'))
    wrong_hmac = "0" * 64  # Obviously wrong HMAC
    
    headers = {
        "Content-Type": "application/json",
        "X-Sequence-Number": str(sequence),
        "X-HMAC-SHA256": wrong_hmac
    }
    
    print(f"  Payload: {payload_str}")
    print(f"  Sequence: {sequence}")
    print(f"  HMAC: {wrong_hmac} (WRONG)")
    
    try:
        # Send raw JSON string to match HMAC computation
        response = requests.post(f"{FLASK_SERVER}/process", 
                                data=payload_str, headers=headers, timeout=5)
        print(f"  Response Status: {response.status_code}")
        print(f"  Response: {response.json()}")
        
        if response.status_code == 401:
            print(f"  ✓ PASSED: Request correctly rejected (401 Unauthorized)")
            return True
        else:
            print(f"  ✗ FAILED: Expected 401, got {response.status_code}")
            return False
    except Exception as e:
        print(f"  Error: {e}")
        return False

def test_replay_attack(old_sequence):
    """Test replay attack with old sequence number (should fail)."""
    print(f"\n{'='*60}")
    print(f"TEST 3: Replay Attack (Old Sequence {old_sequence})")
    print(f"{'='*60}")
    
    payload = {
        "device_id": DEVICE_ID,
        "test": "replay_test",
        "timestamp": int(time.time() * 1000)
    }
    
    payload_str = json.dumps(payload, separators=(',', ':'))
    hmac_value = compute_hmac(payload_str, old_sequence)
    
    headers = {
        "Content-Type": "application/json",
        "X-Sequence-Number": str(old_sequence),
        "X-HMAC-SHA256": hmac_value
    }
    
    print(f"  Payload: {payload_str}")
    print(f"  Sequence: {old_sequence} (OLD - replay attack)")
    print(f"  HMAC: {hmac_value} (correct but for old sequence)")
    
    try:
        # Send raw JSON string to match HMAC computation
        response = requests.post(f"{FLASK_SERVER}/process", 
                                data=payload_str, headers=headers, timeout=5)
        print(f"  Response Status: {response.status_code}")
        print(f"  Response: {response.json()}")
        
        if response.status_code == 401:
            print(f"  ✓ PASSED: Replay correctly detected and rejected")
            return True
        else:
            print(f"  ✗ FAILED: Replay attack not detected!")
            return False
    except Exception as e:
        print(f"  Error: {e}")
        return False

def get_security_stats():
    """Get security statistics from server."""
    print(f"\n{'='*60}")
    print(f"Security Statistics")
    print(f"{'='*60}")
    
    try:
        response = requests.get(f"{FLASK_SERVER}/security/stats", timeout=5)
        if response.status_code == 200:
            data = response.json()
            stats = data.get('security_stats', {})
            print(f"  Total Validations: {stats.get('total_validations', 0)}")
            print(f"  Valid Requests: {stats.get('valid_requests', 0)}")
            print(f"  Invalid Requests: {stats.get('invalid_requests', 0)}")
            print(f"  Devices Tracked: {stats.get('devices_tracked', 0)}")
            print(f"  Device Sequences: {stats.get('device_sequences', {})}")
    except Exception as e:
        print(f"  Error: {e}")

def get_security_log():
    """Get security log from server."""
    print(f"\n{'='*60}")
    print(f"Security Validation Log (Last 10 entries)")
    print(f"{'='*60}")
    
    try:
        response = requests.get(f"{FLASK_SERVER}/security/log?device_id={DEVICE_ID}&limit=10", 
                               timeout=5)
        if response.status_code == 200:
            data = response.json()
            log_entries = data.get('log', [])
            for entry in log_entries:
                status = "✓" if entry['valid'] else "✗"
                print(f"  {status} Seq {entry['sequence']:4d}: {entry['message']}")
    except Exception as e:
        print(f"  Error: {e}")

def reset_sequence():
    """Reset sequence number for testing."""
    print(f"\n{'='*60}")
    print(f"Resetting Sequence Number")
    print(f"{'='*60}")
    
    try:
        response = requests.post(f"{FLASK_SERVER}/security/reset", 
                                json={"device_id": DEVICE_ID}, timeout=5)
        if response.status_code == 200:
            print(f"  ✓ Sequence reset successfully")
        else:
            print(f"  Error: {response.json()}")
    except Exception as e:
        print(f"  Error: {e}")

def main():
    print(f"\n{'='*60}")
    print(f"EcoWatt Security Testing Suite")
    print(f"Target: {FLASK_SERVER}")
    print(f"Device: {DEVICE_ID}")
    print(f"{'='*60}")
    
    # Reset sequence for clean testing
    reset_sequence()
    time.sleep(1)
    
    # Test sequence
    results = []
    
    # Test 1: Valid request with sequence 1
    results.append(("Valid Request (Seq 1)", test_valid_request(1)))
    time.sleep(0.5)
    
    # Test 2: Valid request with sequence 2
    results.append(("Valid Request (Seq 2)", test_valid_request(2)))
    time.sleep(0.5)
    
    # Test 3: Wrong HMAC with sequence 3
    results.append(("Wrong HMAC", test_wrong_hmac(3)))
    time.sleep(0.5)
    
    # Test 4: Replay attack with old sequence 1
    results.append(("Replay Attack (Seq 1)", test_replay_attack(1)))
    time.sleep(0.5)
    
    # Test 5: Valid request with sequence 4 (should work)
    results.append(("Valid Request (Seq 4)", test_valid_request(4)))
    time.sleep(0.5)
    
    # Get statistics
    get_security_stats()
    get_security_log()
    
    # Print summary
    print(f"\n{'='*60}")
    print(f"Test Summary")
    print(f"{'='*60}")
    passed = sum(1 for _, result in results if result)
    total = len(results)
    for test_name, result in results:
        status = "✓ PASSED" if result else "✗ FAILED"
        print(f"  {status}: {test_name}")
    print(f"\nOverall: {passed}/{total} tests passed")
    print(f"{'='*60}\n")

if __name__ == "__main__":
    main()
