"""
OTA (Over-The-Air) firmware update handler for EcoWatt Flask server
Manages firmware updates, chunking, and session tracking
"""

import logging
import threading
from datetime import datetime
from typing import Dict, Optional, Tuple
from pathlib import Path
import json

logger = logging.getLogger(__name__)

# Firmware directory
FIRMWARE_DIR = Path('./firmware')


def _get_latest_firmware_version() -> Optional[str]:
    """Get the latest firmware version from manifest files"""
    try:
        if not FIRMWARE_DIR.exists():
            return None
        
        # Look for manifest files
        manifests = list(FIRMWARE_DIR.glob('*_manifest.json'))
        if not manifests:
            return None
        
        # Get most recent manifest
        latest_manifest = max(manifests, key=lambda p: p.stat().st_mtime)
        
        with open(latest_manifest, 'r') as f:
            manifest_data = json.load(f)
            return manifest_data.get('version')
    except Exception as e:
        logger.error(f"Error getting latest firmware version: {e}")
        return None


def _get_firmware_manifest(version: str) -> Optional[Dict]:
    """Get firmware manifest for a specific version"""
    try:
        manifest_file = FIRMWARE_DIR / f"firmware_{version}_manifest.json"
        
        if not manifest_file.exists():
            return None
        
        with open(manifest_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        logger.error(f"Error loading firmware manifest: {e}")
        return None


def _get_firmware_chunk(version: str, chunk_index: int) -> Optional[bytes]:
    """Get a specific firmware chunk"""
    try:
        # Look for encrypted firmware file
        firmware_file = FIRMWARE_DIR / f"firmware_{version}_encrypted.bin"
        
        if not firmware_file.exists():
            # Try unencrypted
            firmware_file = FIRMWARE_DIR / f"firmware_v{version}.bin"
        
        if not firmware_file.exists():
            return None
        
        # Load manifest to get chunk size
        manifest = _get_firmware_manifest(version)
        chunk_size = manifest.get('chunk_size', 4096) if manifest else 4096
        
        # Read the specific chunk
        with open(firmware_file, 'rb') as f:
            f.seek(chunk_index * chunk_size)
            chunk_data = f.read(chunk_size)
            
        return chunk_data if chunk_data else None
        
    except Exception as e:
        logger.error(f"Error reading firmware chunk: {e}")
        return None


# OTA session tracking
ota_sessions = {}  # device_id -> session info
ota_lock = threading.Lock()

# OTA statistics
ota_stats = {
    'total_updates_initiated': 0,
    'successful_updates': 0,
    'failed_updates': 0,
    'active_sessions': 0,
    'total_bytes_transferred': 0,
    'last_reset': datetime.now().isoformat()
}


def check_for_update(device_id: str, current_version: str) -> Tuple[bool, Optional[Dict]]:
    """
    Check if firmware update is available for device
    
    Args:
        device_id: Device identifier
        current_version: Current firmware version on device
        
    Returns:
        tuple: (update_available, update_info)
    """
    try:
        latest_version = _get_latest_firmware_version()
        
        # Compare versions (simple string comparison, can be enhanced)
        update_available = latest_version != current_version
        
        if update_available:
            manifest = _get_firmware_manifest(latest_version)
            
            if manifest:
                update_info = {
                    'available': True,
                    'latest_version': latest_version,
                    'current_version': current_version,
                    'firmware_size': manifest.get('size', 0),
                    'chunk_size': manifest.get('chunk_size', 4096),
                    'total_chunks': manifest.get('total_chunks', 0),
                    'signature': manifest.get('signature', ''),
                    'release_notes': manifest.get('release_notes', '')
                }
                
                logger.info(f"Update available for {device_id}: "
                          f"{current_version} -> {latest_version}")
                return True, update_info
        
        logger.info(f"No update available for {device_id} (current: {current_version})")
        return False, {'available': False, 'current_version': current_version}
        
    except Exception as e:
        logger.error(f"Error checking for update: {e}")
        return False, {'error': str(e)}


def initiate_ota_session(device_id: str, firmware_version: str) -> Tuple[bool, Optional[str]]:
    """
    Initiate an OTA update session
    
    Args:
        device_id: Device identifier
        firmware_version: Firmware version to update to
        
    Returns:
        tuple: (success, session_id or error_message)
    """
    try:
        with ota_lock:
            # Check if session already exists
            if device_id in ota_sessions:
                existing_session = ota_sessions[device_id]
                if existing_session['status'] == 'in_progress':
                    logger.warning(f"OTA session already in progress for {device_id}")
                    return False, "OTA session already in progress"
            
            # Get firmware manifest
            manifest = _get_firmware_manifest(firmware_version)
            if not manifest:
                return False, f"Firmware version {firmware_version} not found"
            
            # Create session
            session_id = f"{device_id}_{firmware_version}_{int(datetime.now().timestamp())}"
            
            ota_sessions[device_id] = {
                'session_id': session_id,
                'device_id': device_id,
                'firmware_version': firmware_version,
                'status': 'in_progress',
                'current_chunk': 0,
                'total_chunks': manifest.get('total_chunks', 0),
                'bytes_transferred': 0,
                'started_at': datetime.now().isoformat(),
                'last_activity': datetime.now().isoformat()
            }
            
            # Update stats
            ota_stats['total_updates_initiated'] += 1
            ota_stats['active_sessions'] += 1
        
        logger.info(f"OTA session initiated for {device_id}: {session_id}")
        return True, session_id
        
    except Exception as e:
        logger.error(f"Error initiating OTA session: {e}")
        return False, str(e)


def get_firmware_chunk_for_device(
    device_id: str,
    firmware_version: str,
    chunk_index: int
) -> Tuple[bool, Optional[bytes], Optional[str]]:
    """
    Get a firmware chunk for a device
    
    Args:
        device_id: Device identifier
        firmware_version: Firmware version
        chunk_index: Chunk index to retrieve
        
    Returns:
        tuple: (success, chunk_data, error_message)
    """
    try:
        # Verify session exists
        with ota_lock:
            if device_id not in ota_sessions:
                return False, None, "No active OTA session"
            
            session = ota_sessions[device_id]
            
            # Verify firmware version matches
            if session['firmware_version'] != firmware_version:
                return False, None, "Firmware version mismatch"
            
            # Verify chunk index
            if chunk_index >= session['total_chunks']:
                return False, None, f"Invalid chunk index: {chunk_index}"
        
        # Get chunk from firmware manager
        chunk_data = _get_firmware_chunk(firmware_version, chunk_index)
        
        if chunk_data is None:
            return False, None, f"Failed to retrieve chunk {chunk_index}"
        
        # Update session
        with ota_lock:
            session['current_chunk'] = chunk_index
            session['bytes_transferred'] += len(chunk_data)
            session['last_activity'] = datetime.now().isoformat()
            
            ota_stats['total_bytes_transferred'] += len(chunk_data)
        
        logger.debug(f"Sent chunk {chunk_index} to {device_id} ({len(chunk_data)} bytes)")
        return True, chunk_data, None
        
    except Exception as e:
        logger.error(f"Error getting firmware chunk: {e}")
        return False, None, str(e)


def complete_ota_session(device_id: str, success: bool) -> bool:
    """
    Complete an OTA session
    
    Args:
        device_id: Device identifier
        success: Whether update was successful
        
    Returns:
        bool: True if session completed successfully
    """
    try:
        with ota_lock:
            if device_id not in ota_sessions:
                logger.warning(f"No OTA session found for {device_id}")
                return False
            
            session = ota_sessions[device_id]
            session['status'] = 'completed' if success else 'failed'
            session['completed_at'] = datetime.now().isoformat()
            
            # Update stats
            if success:
                ota_stats['successful_updates'] += 1
            else:
                ota_stats['failed_updates'] += 1
            
            ota_stats['active_sessions'] -= 1
            
            # Remove session after completion
            del ota_sessions[device_id]
        
        status = 'successful' if success else 'failed'
        logger.info(f"OTA session {status} for {device_id}")
        return True
        
    except Exception as e:
        logger.error(f"Error completing OTA session: {e}")
        return False


def get_ota_status(device_id: Optional[str] = None) -> Dict:
    """
    Get OTA session status
    
    Args:
        device_id: Device identifier (None = all devices)
        
    Returns:
        dict: OTA status information
    """
    try:
        with ota_lock:
            if device_id:
                # Status for specific device
                if device_id not in ota_sessions:
                    return {'error': 'No active OTA session'}
                
                session = ota_sessions[device_id].copy()
                
                # Calculate progress
                if session['total_chunks'] > 0:
                    session['progress_percent'] = round(
                        (session['current_chunk'] / session['total_chunks']) * 100, 2
                    )
                
                return session
            else:
                # Status for all devices
                all_sessions = {}
                for dev_id, session in ota_sessions.items():
                    session_copy = session.copy()
                    
                    # Calculate progress
                    if session_copy['total_chunks'] > 0:
                        session_copy['progress_percent'] = round(
                            (session_copy['current_chunk'] / session_copy['total_chunks']) * 100, 2
                        )
                    
                    all_sessions[dev_id] = session_copy
                
                return {
                    'active_sessions': len(all_sessions),
                    'sessions': all_sessions
                }
        
    except Exception as e:
        logger.error(f"Error getting OTA status: {e}")
        return {'error': str(e)}


def get_ota_stats() -> Dict:
    """
    Get OTA statistics
    
    Returns:
        dict: OTA statistics
    """
    try:
        with ota_lock:
            stats_copy = ota_stats.copy()
            
            # Calculate success rate
            total = stats_copy['total_updates_initiated']
            if total > 0:
                stats_copy['success_rate'] = round(
                    (stats_copy['successful_updates'] / total) * 100, 2
                )
            else:
                stats_copy['success_rate'] = 0.0
        
        logger.debug(f"Retrieved OTA stats: {stats_copy}")
        return stats_copy
        
    except Exception as e:
        logger.error(f"Error getting OTA stats: {e}")
        return {'error': str(e)}


def cancel_ota_session(device_id: str) -> bool:
    """
    Cancel an active OTA session
    
    Args:
        device_id: Device identifier
        
    Returns:
        bool: True if cancelled successfully
    """
    try:
        with ota_lock:
            if device_id not in ota_sessions:
                logger.warning(f"No OTA session found for {device_id}")
                return False
            
            session = ota_sessions[device_id]
            session['status'] = 'cancelled'
            session['completed_at'] = datetime.now().isoformat()
            
            # Update stats
            ota_stats['failed_updates'] += 1
            ota_stats['active_sessions'] -= 1
            
            # Remove session
            del ota_sessions[device_id]
        
        logger.info(f"OTA session cancelled for {device_id}")
        return True
        
    except Exception as e:
        logger.error(f"Error cancelling OTA session: {e}")
        return False


# ==============================================================================
# FOTA FAULT INJECTION (For Testing)
# ==============================================================================

# Fault injection state
ota_fault_injection = {
    'enabled': False,
    'fault_type': None,
    'target_device': None,
    'target_chunk': None,
    'fault_count': 0
}

ota_fault_lock = threading.Lock()


def enable_ota_fault_injection(fault_type: str, target_device: Optional[str] = None, 
                               target_chunk: Optional[int] = None) -> Tuple[bool, str]:
    """
    Enable OTA fault injection for testing
    
    Args:
        fault_type: Type of fault ('corrupt_chunk', 'bad_hash', 'bad_hmac', 'timeout')
        target_device: Optional device ID to target (None = all devices)
        target_chunk: Optional chunk number to corrupt (None = random)
        
    Returns:
        tuple: (success, message)
    """
    with ota_fault_lock:
        valid_types = ['corrupt_chunk', 'bad_hash', 'bad_hmac', 'timeout', 'incomplete']
        
        if fault_type not in valid_types:
            return False, f"Invalid fault type. Must be one of: {valid_types}"
        
        ota_fault_injection['enabled'] = True
        ota_fault_injection['fault_type'] = fault_type
        ota_fault_injection['target_device'] = target_device
        ota_fault_injection['target_chunk'] = target_chunk
        ota_fault_injection['fault_count'] = 0
        
        logger.warning(f"⚠️  OTA Fault Injection ENABLED: {fault_type}")
        if target_device:
            logger.warning(f"   Target Device: {target_device}")
        if target_chunk is not None:
            logger.warning(f"   Target Chunk: {target_chunk}")
        
        return True, f"Fault injection enabled: {fault_type}"


def disable_ota_fault_injection() -> Tuple[bool, str]:
    """Disable OTA fault injection"""
    with ota_fault_lock:
        was_enabled = ota_fault_injection['enabled']
        fault_count = ota_fault_injection['fault_count']
        
        ota_fault_injection['enabled'] = False
        ota_fault_injection['fault_type'] = None
        ota_fault_injection['target_device'] = None
        ota_fault_injection['target_chunk'] = None
        ota_fault_injection['fault_count'] = 0
        
        if was_enabled:
            logger.info(f"✓ OTA Fault Injection DISABLED (Faults injected: {fault_count})")
            return True, f"Fault injection disabled (Faults injected: {fault_count})"
        else:
            return True, "Fault injection was not enabled"


def get_ota_fault_status() -> Dict:
    """Get current fault injection status"""
    with ota_fault_lock:
        return {
            'enabled': ota_fault_injection['enabled'],
            'fault_type': ota_fault_injection['fault_type'],
            'target_device': ota_fault_injection['target_device'],
            'target_chunk': ota_fault_injection['target_chunk'],
            'fault_count': ota_fault_injection['fault_count']
        }


def _should_inject_fault(device_id: str, chunk_number: Optional[int] = None) -> bool:
    """Check if fault should be injected for this request"""
    if not ota_fault_injection['enabled']:
        return False
    
    # Check device filter
    if ota_fault_injection['target_device'] and ota_fault_injection['target_device'] != device_id:
        return False
    
    # Check chunk filter
    if chunk_number is not None:
        if ota_fault_injection['target_chunk'] is not None:
            if ota_fault_injection['target_chunk'] != chunk_number:
                return False
    
    return True


def _inject_chunk_corruption(chunk_data: Dict) -> Dict:
    """Corrupt chunk data for testing"""
    import base64
    import random
    
    ota_fault_injection['fault_count'] += 1
    logger.warning(f"⚠️  Injecting chunk corruption (fault #{ota_fault_injection['fault_count']})")
    
    # Corrupt the data
    data_bytes = base64.b64decode(chunk_data['data'])
    corrupted = bytearray(data_bytes)
    
    # Flip some random bits
    for _ in range(5):
        pos = random.randint(0, len(corrupted) - 1)
        corrupted[pos] ^= 0xFF
    
    chunk_data['data'] = base64.b64encode(bytes(corrupted)).decode('utf-8')
    return chunk_data


# Export functions
__all__ = [
    'check_for_update',
    'initiate_ota_session',
    'get_firmware_chunk_for_device',
    'complete_ota_session',
    'get_ota_status',
    'get_ota_stats',
    'cancel_ota_session',
    'enable_ota_fault_injection',
    'disable_ota_fault_injection',
    'get_ota_fault_status'
]

