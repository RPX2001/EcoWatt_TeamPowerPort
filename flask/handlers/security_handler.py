"""
Security handler for EcoWatt Flask server
Manages payload validation, nonce tracking, and security statistics

This module provides:
- HMAC-SHA256 verification
- Anti-replay protection with nonce tracking
- Security statistics and logging
- Device tracking

Author: EcoWatt Team
Date: October 28, 2025
"""

import logging
import threading
import time
import hmac
import hashlib
import base64
import json
import os
from datetime import datetime
from typing import Dict, Optional, Tuple, Set

logger = logging.getLogger(__name__)

# ============================================================================
# SECURITY LAYER CORE (copied from server_security_layer.py)
# ============================================================================

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
nonce_lock_core = threading.Lock()


class SecurityError(Exception):
    """Custom exception for security-related errors"""
    pass


def load_nonce_state():
    """Load nonce state from persistent storage"""
    global last_valid_nonce
    
    try:
        if os.path.exists(NONCE_STATE_FILE):
            with open(NONCE_STATE_FILE, 'r') as f:
                data = json.load(f)
                with nonce_lock_core:
                    last_valid_nonce = data.get('nonces', {})
                    # Convert string keys back to proper format if needed
                    for device_id in last_valid_nonce:
                        if isinstance(last_valid_nonce[device_id], dict):
                            last_valid_nonce[device_id] = last_valid_nonce[device_id].get('last_nonce', 0)
                logger.info(f"✓ Loaded nonce state for {len(last_valid_nonce)} device(s)")
    except Exception as e:
        logger.warning(f"Could not load nonce state: {e}")
        last_valid_nonce = {}


def save_nonce_state():
    """Save nonce state to persistent storage"""
    try:
        with nonce_lock_core:
            data = {
                'nonces': last_valid_nonce,
                'last_updated': datetime.utcnow().isoformat()
            }
        
        with open(NONCE_STATE_FILE, 'w') as f:
            json.dump(data, f, indent=2)
        
    except Exception as e:
        logger.warning(f"Could not save nonce state: {e}")


# Load nonce state on module import
load_nonce_state()

# Security statistics tracking (core layer)
core_security_stats = {
    'total_requests': 0,
    'valid_requests': 0,
    'replay_blocked': 0,
    'mac_failed': 0,
    'malformed_requests': 0,
    'device_stats': {}
}
core_security_stats_lock = threading.Lock()


def log_security_event(event_type: str, device_id: str, details: str = ""):
    """
    Log a security event and update statistics.
    
    Args:
        event_type: 'VALID', 'REPLAY', 'MAC_FAIL', 'MALFORMED'
        device_id: Device identifier
        details: Additional event details
    """
    with core_security_stats_lock:
        core_security_stats['total_requests'] += 1
        
        if device_id not in core_security_stats['device_stats']:
            core_security_stats['device_stats'][device_id] = {
                'first_seen': datetime.utcnow().isoformat(),
                'total_requests': 0,
                'valid_requests': 0,
                'replay_blocked': 0,
                'mac_failed': 0,
                'malformed_requests': 0,
                'last_valid_request': None
            }
        
        device_stats = core_security_stats['device_stats'][device_id]
        device_stats['total_requests'] += 1
        
        if event_type == 'VALID':
            core_security_stats['valid_requests'] += 1
            device_stats['valid_requests'] += 1
            device_stats['last_valid_request'] = datetime.utcnow().isoformat()
        elif event_type == 'REPLAY':
            core_security_stats['replay_blocked'] += 1
            device_stats['replay_blocked'] += 1
        elif event_type == 'MAC_FAIL':
            core_security_stats['mac_failed'] += 1
            device_stats['mac_failed'] += 1
        elif event_type == 'MALFORMED':
            core_security_stats['malformed_requests'] += 1
            device_stats['malformed_requests'] += 1
    
    # Log to file
    try:
        with open('security_audit.log', 'a') as f:
            timestamp = datetime.utcnow().isoformat()
            f.write(f"{timestamp} | {device_id} | {event_type} | {details}\n")
    except:
        pass  # Don't fail if logging fails


def get_core_security_stats(device_id: Optional[str] = None) -> dict:
    """
    Get security statistics from core layer.
    
    Args:
        device_id: Optional device ID to get device-specific stats
        
    Returns:
        Dictionary of statistics
    """
    with core_security_stats_lock:
        if device_id:
            return {
                'device_id': device_id,
                **core_security_stats['device_stats'].get(device_id, {})
            }
        else:
            return dict(core_security_stats)


def verify_secured_payload_core(secured_data: str, device_id: str) -> str:
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
        
        logger.debug(f"[Security] Verifying payload: nonce={nonce}, encrypted={encrypted}")
        
        # Step 1: Anti-replay protection - Check nonce
        with nonce_lock_core:
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
        
        # Step 3: Decrypt if encrypted
        if encrypted:
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
        
        # Debug logging
        logger.info(f"[Security Debug] Device: {device_id}")
        logger.info(f"[Security Debug] Nonce: {nonce}")
        logger.info(f"[Security Debug] Payload (decoded): {payload_str[:100]}...")
        logger.info(f"[Security Debug] Payload length: {len(payload_str)}")
        
        # Step 4: Calculate HMAC for verification
        nonce_bytes = nonce.to_bytes(4, 'big')
        data_to_sign = nonce_bytes + payload_str.encode('utf-8')
        mac_calculated = hmac.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
        
        logger.info(f"[Security Debug] HMAC calculated: {mac_calculated}")
        logger.info(f"[Security Debug] HMAC received: {mac_received}")
        
        # Step 5: Verify HMAC
        if not hmac.compare_digest(mac_calculated, mac_received):
            log_security_event('MAC_FAIL', device_id, f"MAC mismatch")
            raise SecurityError(
                f"HMAC verification failed! Calculated: {mac_calculated[:16]}..., "
                f"Received: {mac_received[:16]}..."
            )
        
        # Step 6: Update nonce tracker
        with nonce_lock_core:
            last_valid_nonce[device_id] = nonce
        
        # Save nonce state to persistent storage
        save_nonce_state()
        
        # Log successful verification
        log_security_event('VALID', device_id, f"Nonce: {nonce}")
        
        logger.debug(f"[Security] ✓ Verification successful: nonce={nonce}")
        return payload_str
        
    except json.JSONDecodeError as e:
        raise SecurityError(f"Invalid JSON in secured payload: {e}")
    except KeyError as e:
        raise SecurityError(f"Missing field in secured payload: {e}")
    except Exception as e:
        if isinstance(e, SecurityError):
            raise
        raise SecurityError(f"Unexpected error during verification: {e}")


def reset_nonce_core(device_id: str) -> None:
    """
    Reset nonce tracking for a device (for testing/debugging).
    
    Args:
        device_id: Device identifier
    """
    with nonce_lock_core:
        if device_id in last_valid_nonce:
            del last_valid_nonce[device_id]
            logger.info(f"[Security] Nonce reset for device: {device_id}")


def clear_all_nonces() -> None:
    """
    Clear all nonce tracking data (for testing/debugging).
    This resets the anti-replay protection state for all devices.
    """
    with nonce_lock_core:
        last_valid_nonce.clear()
        logger.info("[Security] All nonce tracking data cleared")


# ============================================================================
# HANDLER LAYER (wraps core security functions)
# ============================================================================

# Security statistics and nonce storage
security_stats = {
    'total_validations': 0,
    'successful_validations': 0,
    'failed_validations': 0,
    'replay_attacks_blocked': 0,
    'signature_failures': 0,
    'crc_failures': 0,
    'unique_devices': set(),
    'last_reset': datetime.now().isoformat()
}

# Nonce tracking (in-memory, can be replaced with Redis/database)
nonce_store = {}  # device_id -> set of used nonces
nonce_lock = threading.Lock()

# Nonce expiry configuration (seconds)
NONCE_EXPIRY_TIME = 3600  # 1 hour
NONCE_CLEANUP_INTERVAL = 600  # 10 minutes


def validate_secured_payload(
    device_id: str,
    payload_json: str
) -> Tuple[bool, Optional[str], Optional[str]]:
    """
    Validate a secured payload with replay protection
    
    Args:
        device_id: Device identifier
        payload_json: Secured payload as JSON string
        
    Returns:
        tuple: (success, decrypted_payload, error_message)
    """
    try:
        # Update statistics
        _update_stat('total_validations')
        _track_device(device_id)
        
        # Call core security layer
        try:
            decrypted_payload = verify_secured_payload_core(payload_json, device_id)
            
            # Success
            _update_stat('successful_validations')
            logger.info(f"Security validation successful for {device_id}")
            return True, decrypted_payload, None
            
        except Exception as verify_error:
            error_msg = str(verify_error)
            _update_stat('failed_validations')
            
            # Categorize failure
            if 'signature' in error_msg.lower() or 'mac' in error_msg.lower():
                _update_stat('signature_failures')
            elif 'crc' in error_msg.lower():
                _update_stat('crc_failures')
            elif 'replay' in error_msg.lower() or 'nonce' in error_msg.lower():
                _update_stat('replay_attacks_blocked')
            
            logger.warning(f"Security validation failed for {device_id}: {error_msg}")
            return False, None, error_msg
        
    except Exception as e:
        _update_stat('failed_validations')
        error_msg = f"Security validation exception: {e}"
        logger.error(f"Error validating payload for {device_id}: {e}")
        return False, None, error_msg


def get_security_stats() -> Dict:
    """
    Get security statistics
    
    Returns:
        dict: Security statistics including success/failure counts
    """
    try:
        with nonce_lock:
            stats_copy = security_stats.copy()
            
            # Convert set to count
            stats_copy['unique_devices'] = len(security_stats['unique_devices'])
            
            # Calculate success rate
            total = stats_copy['total_validations']
            if total > 0:
                stats_copy['success_rate'] = round(
                    (stats_copy['successful_validations'] / total) * 100, 2
                )
            else:
                stats_copy['success_rate'] = 0.0
            
            # Add nonce tracking stats
            stats_copy['devices_tracked'] = len(nonce_store)
            stats_copy['total_nonces'] = sum(len(nonces) for nonces in nonce_store.values())
        
        logger.debug(f"Retrieved security stats: {stats_copy}")
        return stats_copy
        
    except Exception as e:
        logger.error(f"Error getting security stats: {e}")
        return {'error': str(e)}


def reset_security_stats() -> bool:
    """
    Reset security statistics
    
    Returns:
        bool: True if reset successfully
    """
    try:
        with nonce_lock:
            global security_stats
            security_stats = {
                'total_validations': 0,
                'successful_validations': 0,
                'failed_validations': 0,
                'replay_attacks_blocked': 0,
                'signature_failures': 0,
                'crc_failures': 0,
                'unique_devices': set(),
                'last_reset': datetime.now().isoformat()
            }
        
        logger.info("Security statistics reset")
        return True
        
    except Exception as e:
        logger.error(f"Error resetting security stats: {e}")
        return False


def clear_nonces(device_id: Optional[str] = None) -> bool:
    """
    Clear stored nonces for a device or all devices
    
    Args:
        device_id: Device identifier (None = clear all)
        
    Returns:
        bool: True if cleared successfully
    """
    try:
        with nonce_lock:
            if device_id is None:
                # Clear our tracking
                count = len(nonce_store)
                nonce_store.clear()
                logger.info(f"Cleared local nonce tracking for all {count} devices")
            else:
                # Clear specific device
                if device_id in nonce_store:
                    del nonce_store[device_id]
                # Also clear in core security layer
                reset_nonce_core(device_id)
                logger.info(f"Cleared nonces for device: {device_id}")
        
        return True
        
    except Exception as e:
        logger.error(f"Error clearing nonces: {e}")
        return False


def get_device_security_info(device_id: str) -> Dict:
    """
    Get security information for a specific device
    
    Args:
        device_id: Device identifier
        
    Returns:
        dict: Device security details
    """
    try:
        with nonce_lock:
            nonce_count = len(nonce_store.get(device_id, set()))
            
        # Get stats from core security layer
        device_stats = get_core_security_stats(device_id)
        
        return {
            'device_id': device_id,
            'nonces_tracked': nonce_count,
            'stats': device_stats
        }
        
    except Exception as e:
        logger.error(f"Error getting device security info: {e}")
        return {'error': str(e)}


# Internal helper functions

def _is_replay_attack(device_id: str, nonce: int) -> bool:
    """
    Check if nonce has been used before (replay attack detection)
    
    Args:
        device_id: Device identifier
        nonce: Nonce value to check
        
    Returns:
        bool: True if replay attack detected
    """
    with nonce_lock:
        if device_id not in nonce_store:
            return False
        
        return nonce in nonce_store[device_id]


def _store_nonce(device_id: str, nonce: int):
    """
    Store nonce to prevent replay attacks
    
    Args:
        device_id: Device identifier
        nonce: Nonce value to store
    """
    with nonce_lock:
        if device_id not in nonce_store:
            nonce_store[device_id] = set()
        
        nonce_store[device_id].add(nonce)
        
        # Limit nonce storage per device
        if len(nonce_store[device_id]) > 1000:
            # Remove oldest nonces (simple FIFO)
            old_nonces = list(nonce_store[device_id])[:500]
            nonce_store[device_id] -= set(old_nonces)


def _update_stat(stat_name: str):
    """
    Update a security statistic
    
    Args:
        stat_name: Name of statistic to increment
    """
    with nonce_lock:
        if stat_name in security_stats:
            security_stats[stat_name] += 1


def _track_device(device_id: str):
    """
    Track unique device
    
    Args:
        device_id: Device identifier
    """
    with nonce_lock:
        security_stats['unique_devices'].add(device_id)


# Cleanup task (should be run periodically)
def cleanup_expired_nonces():
    """
    Remove expired nonces (for periodic cleanup task)
    This should be called by a background task/scheduler
    """
    try:
        with nonce_lock:
            # Simple cleanup: clear all nonces periodically
            # In production, track timestamps and remove only expired ones
            count = len(nonce_store)
            nonce_store.clear()
            
        logger.info(f"Cleaned up nonces for {count} devices")
        
    except Exception as e:
        logger.error(f"Error cleaning up nonces: {e}")


# Export functions
__all__ = [
    'validate_secured_payload',
    'get_security_stats',
    'reset_security_stats',
    'clear_nonces',
    'get_device_security_info',
    'cleanup_expired_nonces'
]
