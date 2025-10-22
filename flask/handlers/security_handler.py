"""
Security handler for EcoWatt Flask server
Manages payload validation, nonce tracking, and security statistics
"""

import logging
import threading
import time
from datetime import datetime
from typing import Dict, Optional, Tuple, Set

# Import existing security module
from server_security_layer import (
    verify_secured_payload as ssl_verify_secured_payload,
    get_security_stats as ssl_get_security_stats
)

logger = logging.getLogger(__name__)

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
        
        # Call existing security layer
        try:
            decrypted_payload = ssl_verify_secured_payload(payload_json, device_id)
            
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
    Note: Nonce management is handled by server_security_layer
    
    Args:
        device_id: Device identifier (None = clear all)
        
    Returns:
        bool: True if cleared successfully
    """
    try:
        # Import and use the existing reset_nonce function
        from server_security_layer import reset_nonce
        
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
                # Also clear in security layer
                reset_nonce(device_id)
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
            
        # Get stats from existing security layer
        device_stats = ssl_get_security_stats(device_id)
        
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
