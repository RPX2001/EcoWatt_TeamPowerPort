"""
Test script for Security Layer integration
This script tests the security layer verification function
"""

import json
import base64
import hmac
import hashlib
import struct

# Pre-shared keys (must match ESP32 and Flask server)
PSK_HMAC = bytes([
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
])

def create_test_secured_payload(json_payload, nonce):
    """
    Create a test secured payload (mimicking ESP32 behavior with encryption disabled)
    """
    # Base64 encode the JSON payload (mock encryption - no real AES)
    encoded_payload = base64.b64encode(json_payload.encode('utf-8')).decode('utf-8')
    
    # Create data to sign: nonce (4 bytes big-endian) + payload
    nonce_bytes = struct.pack('>I', nonce)
    data_to_sign = nonce_bytes + json_payload.encode('utf-8')
    
    # Calculate HMAC-SHA256
    hmac_digest = hmac.new(PSK_HMAC, data_to_sign, hashlib.sha256).digest()
    hmac_hex = hmac_digest.hex()
    
    # Create secured JSON structure
    secured = {
        "nonce": nonce,
        "payload": encoded_payload,
        "mac": hmac_hex,
        "encrypted": False
    }
    
    return json.dumps(secured)

def test_security_layer():
    """Test the security layer with sample data"""
    
    # Create a sample compressed data payload
    sample_payload = {
        "device_id": "ESP32_EcoWatt_Smart",
        "compressed_data": [
            {
                "compressed_binary": "AwMFAAABAAAAAAA=",
                "compression_method": "Dictionary",
                "timestamp": 1234567890
            }
        ],
        "session_summary": {
            "total_packets": 1,
            "total_compressed_bytes": 12,
            "total_original_bytes": 60
        }
    }
    
    json_payload = json.dumps(sample_payload)
    test_nonce = 10001
    
    # Create secured payload
    secured_payload = create_test_secured_payload(json_payload, test_nonce)
    
    print("=" * 80)
    print("SECURITY LAYER TEST")
    print("=" * 80)
    print("\n1. Original JSON Payload:")
    print(json.dumps(sample_payload, indent=2))
    print(f"\n2. Payload size: {len(json_payload)} bytes")
    print(f"\n3. Nonce used: {test_nonce}")
    print("\n4. Secured Payload:")
    print(secured_payload)
    print(f"\n5. Secured payload size: {len(secured_payload)} bytes")
    
    # Parse secured payload to show structure
    secured_data = json.loads(secured_payload)
    print("\n6. Secured Payload Structure:")
    print(f"   - Nonce: {secured_data['nonce']}")
    print(f"   - Payload (base64): {secured_data['payload'][:50]}...")
    print(f"   - MAC (HMAC-SHA256): {secured_data['mac'][:32]}...")
    print(f"   - Encrypted: {secured_data['encrypted']}")
    
    print("\n" + "=" * 80)
    print("To test with Flask server, send this secured payload to /process endpoint")
    print("=" * 80)
    
    return secured_payload

if __name__ == "__main__":
    secured = test_security_layer()
    
    # Save to file for easy testing
    with open('test_secured_payload.json', 'w') as f:
        f.write(secured)
    
    print("\n✓ Test secured payload saved to 'test_secured_payload.json'")
    print("\nYou can test it with:")
    print("  curl -X POST http://localhost:5001/process \\")
    print("       -H 'Content-Type: application/json' \\")
    print("       -d @test_secured_payload.json")
