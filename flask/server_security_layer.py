"""
Security Layer Verification Module for Flask Server

This module handles verification of secured payloads from ESP32 devices.
It provides HMAC-SHA256 verification, anti-replay protection, and optional AES decryption.

Author: EcoWatt Team
Date: October 16, 2025
"""

import hmac
import hashlib
import base64
import json
from threading import Lock
from typing import Tuple, Optional

# Pre-Shared Keys - MUST match ESP32 keys exactly!
PSK_HMAC = bytes([
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
])

PSK_AES = bytes([
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
])

AES_IV = bytes([
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
])

# Nonce tracking for anti-replay protection
last_valid_nonce = {}
nonce_lock = Lock()


class SecurityError(Exception):
    """Custom exception for security-related errors"""
    pass


def verify_secured_payload(secured_data: str, device_id: str) -> str:
    """
    Verify HMAC and decrypt (if encrypted) the secured payload from ESP32.
    
    Args:
        secured_data: JSON string containing secured payload
        device_id: Device identifier for nonce tracking
        
    Returns:
        Original JSON string if verification succeeds
        
    Raises:
        SecurityError: If verification fails
    """
    try:
        # Parse secured JSON
        data = json.loads(secured_data)
        
        # Validate required fields
        required_fields = ['nonce', 'payload', 'mac']
        if not all(field in data for field in required_fields):
            raise SecurityError(f"Missing required security fields. Required: {required_fields}")
        
        nonce = data['nonce']
        payload_b64 = data['payload']
        mac_received = data['mac']
        encrypted = data.get('encrypted', False)
        
        print(f"[Security] Verifying payload: nonce={nonce}, encrypted={encrypted}")
        
        # Step 1: Anti-replay protection - Check nonce
        with nonce_lock:
            if device_id in last_valid_nonce:
                if nonce <= last_valid_nonce[device_id]:
                    raise SecurityError(
                        f"Replay attack detected! Nonce {nonce} <= last valid nonce {last_valid_nonce[device_id]}"
                    )
        
        # Step 2: Decode Base64 payload
        try:
            payload_bytes = base64.b64decode(payload_b64)
        except Exception as e:
            raise SecurityError(f"Failed to decode base64 payload: {e}")
        
        # Step 3: Decrypt if encrypted (currently mock mode uses Base64 only)
        if encrypted:
            # For real AES decryption:
            from Crypto.Cipher import AES
            try:
                cipher = AES.new(PSK_AES, AES.MODE_CBC, AES_IV)
                payload_bytes = cipher.decrypt(payload_bytes)
                # Remove PKCS7 padding
                pad_len = payload_bytes[-1]
                payload_bytes = payload_bytes[:-pad_len]
            except Exception as e:
                raise SecurityError(f"AES decryption failed: {e}")
        
        # Convert to string
        try:
            payload_str = payload_bytes.decode('utf-8')
        except Exception as e:
            raise SecurityError(f"Failed to decode payload to UTF-8: {e}")
        
        # Step 4: Calculate HMAC for verification
        # Data to sign: nonce (4 bytes, big-endian) + original payload
        nonce_bytes = nonce.to_bytes(4, 'big')
        data_to_sign = nonce_bytes + payload_str.encode('utf-8')
        
        mac_calculated = hmac.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
        
        # Step 5: Verify HMAC
        if not hmac.compare_digest(mac_calculated, mac_received):
            raise SecurityError(
                f"HMAC verification failed! Calculated: {mac_calculated[:16]}..., "
                f"Received: {mac_received[:16]}..."
            )
        
        # Step 6: Update nonce tracker
        with nonce_lock:
            last_valid_nonce[device_id] = nonce
        
        print(f"[Security] ✓ Verification successful: nonce={nonce}")
        return payload_str
        
    except json.JSONDecodeError as e:
        raise SecurityError(f"Invalid JSON in secured payload: {e}")
    except KeyError as e:
        raise SecurityError(f"Missing field in secured payload: {e}")
    except Exception as e:
        if isinstance(e, SecurityError):
            raise
        raise SecurityError(f"Unexpected error during verification: {e}")


def is_secured_payload(data: dict) -> bool:
    """
    Check if the payload has security layer applied.
    
    Args:
        data: Parsed JSON dictionary
        
    Returns:
        True if payload has security fields, False otherwise
    """
    return all(key in data for key in ['nonce', 'payload', 'mac'])


def get_last_nonce(device_id: str) -> Optional[int]:
    """
    Get the last valid nonce for a device.
    
    Args:
        device_id: Device identifier
        
    Returns:
        Last valid nonce or None if device not tracked
    """
    with nonce_lock:
        return last_valid_nonce.get(device_id)


def reset_nonce(device_id: str) -> None:
    """
    Reset nonce tracking for a device (for testing/debugging).
    
    Args:
        device_id: Device identifier
    """
    with nonce_lock:
        if device_id in last_valid_nonce:
            del last_valid_nonce[device_id]
            print(f"[Security] Nonce reset for device: {device_id}")


# Test function
def test_security_layer():
    """Test the security layer with a sample payload"""
    print("\n=== Security Layer Test ===\n")
    
    # Sample secured payload (mock)
    test_nonce = 10001
    original_data = '{"device_id":"ESP32_Test","value":123}'
    
    # Encode payload
    payload_b64 = base64.b64encode(original_data.encode('utf-8')).decode('utf-8')
    
    # Calculate HMAC
    nonce_bytes = test_nonce.to_bytes(4, 'big')
    data_to_sign = nonce_bytes + original_data.encode('utf-8')
    mac = hmac.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
    
    # Build secured payload
    secured_payload = {
        "nonce": test_nonce,
        "payload": payload_b64,
        "mac": mac,
        "encrypted": False
    }
    
    print(f"Test Secured Payload:\n{json.dumps(secured_payload, indent=2)}\n")
    
    # Verify
    try:
        result = verify_secured_payload(json.dumps(secured_payload), "ESP32_Test")
        print(f"✓ Verification successful!")
        print(f"Original data: {result}\n")
    except SecurityError as e:
        print(f"✗ Verification failed: {e}\n")


if __name__ == "__main__":
    test_security_layer()
