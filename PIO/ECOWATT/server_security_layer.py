"""
Flask Server Security Layer Implementation
===========================================

This module provides server-side verification of secured payloads
sent from the ESP32 EcoWatt device.

Author: Security Layer Integration Team
Date: October 16, 2025
"""

import hmac
import hashlib
import base64
import json
from typing import Dict, Any, Optional
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad

# Pre-shared keys (MUST match ESP32 keys)
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

# Device-specific nonce tracking (in production, use database)
device_nonces = {}


class SecurityError(Exception):
    """Custom exception for security-related errors"""
    pass


def verify_secured_payload(secured_data: Dict[str, Any], device_id: str) -> Dict[str, Any]:
    """
    Verify and extract original payload from secured data.
    
    Args:
        secured_data: Dictionary containing nonce, payload, mac, and encrypted flag
        device_id: Unique device identifier
        
    Returns:
        Original JSON payload as dictionary
        
    Raises:
        SecurityError: If verification fails
    """
    try:
        # Extract fields
        nonce = secured_data.get('nonce')
        payload_b64 = secured_data.get('payload')
        received_mac = secured_data.get('mac')
        encrypted = secured_data.get('encrypted', False)
        
        if not all([nonce is not None, payload_b64, received_mac]):
            raise SecurityError("Missing required fields in secured payload")
        
        # Anti-replay check
        last_nonce = device_nonces.get(device_id, 0)
        if nonce <= last_nonce:
            raise SecurityError(
                f"Replay attack detected! Received nonce {nonce} <= last seen {last_nonce}"
            )
        
        # Decode payload
        try:
            payload_bytes = base64.b64decode(payload_b64)
        except Exception as e:
            raise SecurityError(f"Failed to decode base64 payload: {e}")
        
        # Decrypt if needed
        if encrypted:
            try:
                payload_bytes = decrypt_aes_cbc(payload_bytes)
            except Exception as e:
                raise SecurityError(f"Decryption failed: {e}")
        
        # Verify HMAC
        nonce_bytes = nonce.to_bytes(4, byteorder='big')
        data_to_sign = nonce_bytes + payload_bytes
        
        calculated_mac = hmac.new(
            PSK_HMAC,
            data_to_sign,
            hashlib.sha256
        ).hexdigest()
        
        if not hmac.compare_digest(calculated_mac, received_mac):
            raise SecurityError(
                f"HMAC verification failed!\n"
                f"Expected: {calculated_mac}\n"
                f"Received: {received_mac}"
            )
        
        # Update nonce (anti-replay protection)
        device_nonces[device_id] = nonce
        
        # Parse original JSON
        try:
            original_data = json.loads(payload_bytes.decode('utf-8'))
        except Exception as e:
            raise SecurityError(f"Failed to parse JSON payload: {e}")
        
        print(f"[SECURITY] Payload verified successfully for {device_id}, nonce: {nonce}")
        return original_data
        
    except SecurityError:
        raise
    except Exception as e:
        raise SecurityError(f"Unexpected error during verification: {e}")


def decrypt_aes_cbc(ciphertext: bytes) -> bytes:
    """
    Decrypt data using AES-128-CBC.
    
    Args:
        ciphertext: Encrypted data
        
    Returns:
        Decrypted plaintext
    """
    cipher = AES.new(PSK_AES, AES.MODE_CBC, AES_IV)
    plaintext = cipher.decrypt(ciphertext)
    
    # Remove PKCS7 padding
    try:
        plaintext = unpad(plaintext, AES.block_size)
    except ValueError:
        # If unpadding fails, return as-is (might be no padding)
        pass
    
    return plaintext


def encrypt_aes_cbc(plaintext: bytes) -> bytes:
    """
    Encrypt data using AES-128-CBC.
    
    Args:
        plaintext: Data to encrypt
        
    Returns:
        Encrypted ciphertext
    """
    cipher = AES.new(PSK_AES, AES.MODE_CBC, AES_IV)
    
    # Apply PKCS7 padding
    plaintext = pad(plaintext, AES.block_size)
    
    ciphertext = cipher.encrypt(plaintext)
    return ciphertext


def secure_response(response_data: Dict[str, Any], device_id: str, 
                   enable_encryption: bool = False) -> Dict[str, Any]:
    """
    Secure a response payload to send back to the device.
    
    Args:
        response_data: Dictionary to secure
        device_id: Unique device identifier
        enable_encryption: Whether to encrypt the payload
        
    Returns:
        Secured payload with nonce, payload, and mac
    """
    # Get next nonce for this device
    current_nonce = device_nonces.get(device_id, 10000)
    next_nonce = current_nonce + 1
    
    # Serialize JSON
    payload_bytes = json.dumps(response_data).encode('utf-8')
    
    # Encrypt if requested
    if enable_encryption:
        encrypted_payload = encrypt_aes_cbc(payload_bytes)
        payload_b64 = base64.b64encode(encrypted_payload).decode('utf-8')
    else:
        payload_b64 = base64.b64encode(payload_bytes).decode('utf-8')
    
    # Calculate HMAC
    nonce_bytes = next_nonce.to_bytes(4, byteorder='big')
    data_to_sign = nonce_bytes + payload_bytes
    
    mac = hmac.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
    
    # Update nonce
    device_nonces[device_id] = next_nonce
    
    return {
        'nonce': next_nonce,
        'payload': payload_b64,
        'mac': mac,
        'encrypted': enable_encryption
    }


# Flask integration example
def create_secured_flask_routes(app):
    """
    Create Flask routes with security layer integration.
    
    Usage:
        from flask import Flask, request, jsonify
        app = Flask(__name__)
        create_secured_flask_routes(app)
    """
    from flask import request, jsonify
    
    @app.route('/process', methods=['POST'])
    def process_secured_data():
        """Handle secured data upload from ESP32"""
        try:
            # Get secured payload
            secured_data = request.get_json()
            
            if not secured_data:
                return jsonify({'error': 'No data provided'}), 400
            
            # Extract device ID from the secured payload
            # (In production, you might verify device ID separately)
            device_id = "ESP32_EcoWatt_Smart"  # This should come from authentication
            
            # Verify and extract original payload
            try:
                original_data = verify_secured_payload(secured_data, device_id)
            except SecurityError as e:
                print(f"[SECURITY ERROR] {e}")
                return jsonify({'error': str(e), 'security_failure': True}), 403
            
            # Process the verified data
            print(f"[DATA] Received from {device_id}:")
            print(f"  Device ID: {original_data.get('device_id')}")
            print(f"  Timestamp: {original_data.get('timestamp')}")
            print(f"  Data Type: {original_data.get('data_type')}")
            print(f"  Total Samples: {original_data.get('total_samples')}")
            
            # Your existing processing logic here
            # ...
            
            # Optionally secure the response
            response_data = {
                'status': 'success',
                'message': 'Data processed successfully',
                'samples_received': original_data.get('total_samples', 0)
            }
            
            # Return normal response (or secured if needed)
            return jsonify(response_data), 200
            
        except Exception as e:
            print(f"[ERROR] {e}")
            return jsonify({'error': str(e)}), 500
    
    @app.route('/changes', methods=['POST'])
    def handle_secured_changes_request():
        """Handle secured configuration changes request"""
        try:
            secured_data = request.get_json()
            
            if not secured_data:
                return jsonify({'error': 'No data provided'}), 400
            
            device_id = "ESP32_EcoWatt_Smart"
            
            # Verify request
            try:
                original_request = verify_secured_payload(secured_data, device_id)
            except SecurityError as e:
                print(f"[SECURITY ERROR] {e}")
                return jsonify({'error': str(e), 'security_failure': True}), 403
            
            print(f"[CONFIG] Changes request from {device_id}")
            
            # Your existing changes logic here
            # ...
            
            response_data = {
                'Changed': False,
                'pollFreqChanged': False,
                'uploadFreqChanged': False,
                'regsChanged': False
            }
            
            # Return unsecured response for now
            # (You can use secure_response() if needed)
            return jsonify(response_data), 200
            
        except Exception as e:
            print(f"[ERROR] {e}")
            return jsonify({'error': str(e)}), 500


# Standalone testing
if __name__ == '__main__':
    # Test encryption/decryption
    print("Testing AES encryption...")
    test_data = b"Hello, EcoWatt!"
    encrypted = encrypt_aes_cbc(test_data)
    decrypted = decrypt_aes_cbc(encrypted)
    assert test_data == decrypted, "AES encryption/decryption test failed"
    print("✓ AES test passed")
    
    # Test HMAC
    print("\nTesting HMAC...")
    test_message = b"Test message"
    test_mac = hmac.new(PSK_HMAC, test_message, hashlib.sha256).hexdigest()
    print(f"✓ HMAC generated: {test_mac}")
    
    # Test secured payload verification
    print("\nTesting secured payload verification...")
    test_payload = {
        'device_id': 'ESP32_EcoWatt_Smart',
        'test': 'data'
    }
    
    secured = secure_response(test_payload, 'test_device', enable_encryption=False)
    print(f"✓ Secured payload created with nonce: {secured['nonce']}")
    
    verified = verify_secured_payload(secured, 'test_device')
    assert verified == test_payload, "Payload verification test failed"
    print("✓ Payload verification test passed")
    
    print("\n✅ All security layer tests passed!")
