"""
Test what requests.post actually sends
"""

import requests
import json
import hmac
import hashlib

PSK_HEX = "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe"
PSK = bytes.fromhex(PSK_HEX)

payload = {
    "device_id": "ESP32_EcoWatt_Smart",
    "test": "security_test",
    "timestamp": 1760623047388
}

# This is what test_security.py does
payload_str = json.dumps(payload, separators=(',', ':'))
print("What we compute HMAC from:")
print(f"  String: {payload_str}")
print(f"  Bytes: {payload_str.encode('utf-8')}")

# Compute HMAC
sequence_bytes = (1).to_bytes(4, byteorder='big')
message = payload_str.encode('utf-8') + sequence_bytes
h = hmac.new(PSK, message, hashlib.sha256)
print(f"  HMAC: {h.hexdigest()}")

print("\nWhat requests.post sends:")
print(f"  It serializes: {json.dumps(payload)}")
print(f"  With separators: {json.dumps(payload, separators=(',', ':'))}")
print(f"  Default separators: {json.dumps(payload, separators=(', ', ': '))}")

# The issue: requests might add spaces!
print("\nChecking if they match:")
print(f"  With compact: {json.dumps(payload, separators=(',', ':'))}")
print(f"  Default:      {json.dumps(payload)}")
print(f"  Match: {json.dumps(payload, separators=(',', ':')) == json.dumps(payload)}")
