"""
Debug HMAC computation
"""

import hmac
import hashlib
import json

PSK_HEX = "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe"
PSK = bytes.fromhex(PSK_HEX)

def compute_hmac(payload, sequence_number):
    """Compute HMAC-SHA256."""
    if isinstance(payload, str):
        payload = payload.encode('utf-8')
    
    # Append sequence number (big-endian, 4 bytes)
    sequence_bytes = sequence_number.to_bytes(4, byteorder='big')
    message = payload + sequence_bytes
    
    print(f"Payload: {payload}")
    print(f"Sequence bytes: {sequence_bytes.hex()}")
    print(f"Message: {message}")
    
    # Compute HMAC
    h = hmac.new(PSK, message, hashlib.sha256)
    return h.hexdigest()

# Test case 1: JSON dict
payload1 = {
    "device_id": "ESP32_EcoWatt_Smart",
    "test": "security_test",
    "timestamp": 1760623047388
}

payload1_str = json.dumps(payload1, separators=(',', ':'))
print("="*60)
print("Test 1: JSON payload")
print("="*60)
print(f"Payload string: {payload1_str}")
hmac1 = compute_hmac(payload1_str, 1)
print(f"HMAC: {hmac1}")
print()

# Test case 2: Same but from bytes (like request.get_data())
print("="*60)
print("Test 2: Same payload as bytes")
print("="*60)
payload1_bytes = payload1_str.encode('utf-8')
print(f"Payload bytes: {payload1_bytes}")
hmac2 = compute_hmac(payload1_bytes, 1)
print(f"HMAC: {hmac2}")
print()

print(f"Match: {hmac1 == hmac2}")
