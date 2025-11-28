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
import os
from datetime import datetime
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
NONCE_STATE_FILE = 'nonce_state.json'
last_valid_nonce = {}
nonce_lock = Lock()


def load_nonce_state():
    """Load nonce state from persistent storage"""
    global last_valid_nonce
    
    try:
        if os.path.exists(NONCE_STATE_FILE):
            with open(NONCE_STATE_FILE, 'r') as f:
                data = json.load(f)
                with nonce_lock:
                    last_valid_nonce = data.get('nonces', {})
                    # Convert string keys back to proper format if needed
                    for device_id in last_valid_nonce:
                        if isinstance(last_valid_nonce[device_id], dict):
                            last_valid_nonce[device_id] = last_valid_nonce[device_id].get('last_nonce', 0)
                print(f"✓ Loaded nonce state for {len(last_valid_nonce)} device(s)")
    except Exception as e:
        print(f"Warning: Could not load nonce state: {e}")
        last_valid_nonce = {}


def save_nonce_state():
    """Save nonce state to persistent storage"""
    try:
        with nonce_lock:
            data = {
                'nonces': last_valid_nonce,
                'last_updated': datetime.utcnow().isoformat()
            }
        
        with open(NONCE_STATE_FILE, 'w') as f:
            json.dump(data, f, indent=2)
        
    except Exception as e:
        print(f"Warning: Could not save nonce state: {e}")


# Load nonce state on module import
load_nonce_state()

# Security statistics tracking
security_stats = {
    'total_requests': 0,
    'valid_requests': 0,
    'replay_blocked': 0,
    'mac_failed': 0,
    'malformed_requests': 0,
    'device_stats': {}
}
security_stats_lock = Lock()


def log_security_event(event_type: str, device_id: str, details: str = ""):
    """
    Log a security event and update statistics.
    
    Args:
        event_type: 'VALID', 'REPLAY', 'MAC_FAIL', 'MALFORMED'
        device_id: Device identifier
        details: Additional event details
    """
    with security_stats_lock:
        security_stats['total_requests'] += 1
        
        if device_id not in security_stats['device_stats']:
            security_stats['device_stats'][device_id] = {
                'first_seen': datetime.utcnow().isoformat(),
                'total_requests': 0,
                'valid_requests': 0,
                'replay_blocked': 0,
                'mac_failed': 0,
                'malformed_requests': 0,
                'last_valid_request': None
            }
        
        device_stats = security_stats['device_stats'][device_id]
        device_stats['total_requests'] += 1
        
        if event_type == 'VALID':
            security_stats['valid_requests'] += 1
            device_stats['valid_requests'] += 1
            device_stats['last_valid_request'] = datetime.utcnow().isoformat()
        elif event_type == 'REPLAY':
            security_stats['replay_blocked'] += 1
            device_stats['replay_blocked'] += 1
        elif event_type == 'MAC_FAIL':
            security_stats['mac_failed'] += 1
            device_stats['mac_failed'] += 1
        elif event_type == 'MALFORMED':
            security_stats['malformed_requests'] += 1
            device_stats['malformed_requests'] += 1
    
    # Log to file
    try:
        with open('security_audit.log', 'a') as f:
            timestamp = datetime.utcnow().isoformat()
            f.write(f"{timestamp} | {device_id} | {event_type} | {details}\n")
    except:
        pass  # Don't fail if logging fails


def get_security_stats(device_id: Optional[str] = None) -> dict:
    """
    Get security statistics.
    
    Args:
        device_id: Optional device ID to get device-specific stats
        
    Returns:
        Dictionary of statistics
    """
    with security_stats_lock:
        if device_id:
            return {
                'device_id': device_id,
                **security_stats['device_stats'].get(device_id, {})
            }
        else:
            return dict(security_stats)


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
            log_security_event('MALFORMED', device_id, "Missing required fields")
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
                    log_security_event('REPLAY', device_id, f"Nonce {nonce} <= {last_valid_nonce[device_id]}")
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
            log_security_event('MAC_FAIL', device_id, f"MAC mismatch")
            raise SecurityError(
                f"HMAC verification failed! Calculated: {mac_calculated[:16]}..., "
                f"Received: {mac_received[:16]}..."
            )
        
        # Step 6: Update nonce tracker
        with nonce_lock:
            last_valid_nonce[device_id] = nonce
        
        # Save nonce state to persistent storage (async to not block)
        save_nonce_state()
        
        # Log successful verification
        log_security_event('VALID', device_id, f"Nonce: {nonce}")
        
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


def main():
    """
    CLI interface for security layer testing.
    
    Usage:
        python server_security_layer.py test                    # Run test
        python server_security_layer.py verify <payload> <device_id>  # Verify a payload
        python server_security_layer.py stats [device_id]       # Show statistics
        python server_security_layer.py reset <device_id>       # Reset nonce for device
    """
    import argparse
    
    parser = argparse.ArgumentParser(
        description='EcoWatt Security Layer - Testing and verification tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run built-in test
  python server_security_layer.py test
  
  # Verify a secured payload
  python server_security_layer.py verify '{"nonce":123,"payload":"...","mac":"..."}' ESP32_Device_01
  
  # Show security statistics
  python server_security_layer.py stats
  python server_security_layer.py stats ESP32_Device_01
  
  # Reset nonce for a device (testing only)
  python server_security_layer.py reset ESP32_Device_01
        """
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # Test command
    subparsers.add_parser('test', help='Run built-in security layer test')
    
    # Verify command
    verify_parser = subparsers.add_parser('verify', help='Verify a secured payload')
    verify_parser.add_argument('payload', help='JSON string of secured payload')
    verify_parser.add_argument('device_id', help='Device identifier')
    
    # Stats command
    stats_parser = subparsers.add_parser('stats', help='Show security statistics')
    stats_parser.add_argument('device_id', nargs='?', help='Optional: device ID for device-specific stats')
    
    # Reset command
    reset_parser = subparsers.add_parser('reset', help='Reset nonce for device (testing only)')
    reset_parser.add_argument('device_id', help='Device identifier to reset')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    try:
        if args.command == 'test':
            test_security_layer()
            return 0
            
        elif args.command == 'verify':
            print(f"\n=== Verifying Payload for {args.device_id} ===\n")
            try:
                result = verify_secured_payload(args.payload, args.device_id)
                print(f"✓ Verification successful!")
                print(f"Decrypted payload:\n{result}\n")
                return 0
            except SecurityError as e:
                print(f"✗ Verification failed: {e}\n")
                return 1
                
        elif args.command == 'stats':
            print("\n=== Security Statistics ===\n")
            stats = get_security_stats(args.device_id)
            print(json.dumps(stats, indent=2))
            print()
            return 0
            
        elif args.command == 'reset':
            print(f"\n=== Resetting Nonce for {args.device_id} ===\n")
            reset_nonce(args.device_id)
            print(f"✓ Nonce reset complete\n")
            return 0
            
    except Exception as e:
        print(f"Error: {e}\n")
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
