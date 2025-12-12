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

# Firmware directory (absolute path relative to this file's parent directory)
FIRMWARE_DIR = Path(__file__).parent.parent / 'firmware'


def _get_latest_firmware_version() -> Optional[str]:
    """Get the latest firmware version from manifest files"""
    try:
        if not FIRMWARE_DIR.exists():
            logger.error(f"Firmware directory does not exist: {FIRMWARE_DIR}")
            return None
        
        # Look for manifest files
        manifests = list(FIRMWARE_DIR.glob('*_manifest.json'))
        logger.info(f"Found {len(manifests)} manifest files: {[m.name for m in manifests]}")
        if not manifests:
            return None
        
        # Get most recent manifest
        latest_manifest = max(manifests, key=lambda p: p.stat().st_mtime)
        logger.info(f"Latest manifest by mtime: {latest_manifest.name} (mtime: {latest_manifest.stat().st_mtime})")
        
        with open(latest_manifest, 'r') as f:
            manifest_data = json.load(f)
            version = manifest_data.get('version')
            logger.info(f"Latest firmware version detected: {version}")
            return version
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
                    'original_size': manifest.get('original_size', 0),
                    'encrypted_size': manifest.get('encrypted_size', 0),
                    'firmware_size': manifest.get('original_size', 0),  # Alias for compatibility
                    'sha256_hash': manifest.get('sha256_hash', ''),
                    'signature': manifest.get('signature', ''),
                    'iv': manifest.get('iv', ''),
                    'chunk_size': manifest.get('chunk_size', 1024),
                    'total_chunks': manifest.get('total_chunks', 0),
                    'release_notes': manifest.get('release_notes', '')
                }
                
                # Milestone 5: Inject manifest faults if enabled (bad_hash or bad_signature)
                if _should_inject_fault(device_id):
                    fault_type = ota_fault_injection['fault_type']
                    if fault_type in ['bad_hash', 'bad_signature']:
                        update_info = _inject_manifest_corruption(update_info)
                
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
        from database import Database
        
        with ota_lock:
            # Check if session already exists
            if device_id in ota_sessions:
                existing_session = ota_sessions[device_id]
                if existing_session['status'] == 'in_progress':
                    # Check if session is stale (older than 10 minutes)
                    import time
                    last_activity = existing_session.get('last_activity')
                    
                    # Handle both string (ISO) and float (timestamp) formats
                    try:
                        if isinstance(last_activity, str):
                            # Parse ISO format timestamp
                            last_activity_time = datetime.fromisoformat(last_activity.replace('Z', '+00:00')).timestamp()
                        elif last_activity:
                            last_activity_time = float(last_activity)
                        else:
                            # No last_activity timestamp - assume stale
                            last_activity_time = 0
                    except:
                        # If parsing fails, assume stale
                        last_activity_time = 0
                    
                    time_since_activity = time.time() - last_activity_time
                    if time_since_activity > 600:  # 10 minutes
                        logger.warning(f"[!] Cleaning up stale OTA session for {device_id} (inactive for {time_since_activity:.0f}s)")
                        del ota_sessions[device_id]
                        if ota_stats['active_sessions'] > 0:
                            ota_stats['active_sessions'] -= 1
                    else:
                        logger.warning(f"[!] OTA session already in progress for {device_id} (active {time_since_activity:.0f}s ago)")
                        return False, "OTA session already in progress"
            
            # Get firmware manifest
            manifest = _get_firmware_manifest(firmware_version)
            if not manifest:
                return False, f"Firmware version {firmware_version} not found"
            
            # Get current firmware version from database
            device = Database.get_device(device_id)
            from_version = device.get('firmware_version', 'unknown') if device else 'unknown'
            
            # Create session
            session_id = f"{device_id}_{firmware_version}_{int(datetime.now().timestamp())}"
            
            import time
            ota_sessions[device_id] = {
                'session_id': session_id,
                'device_id': device_id,
                'firmware_version': firmware_version,
                'status': 'in_progress',
                'current_chunk': 0,
                'total_chunks': manifest.get('total_chunks', 0),
                'bytes_transferred': 0,
                'started_at': datetime.now().isoformat(),
                'last_activity': time.time()  # Use timestamp for easier comparison
            }
            
            # Save to database
            Database.save_ota_update(
                device_id=device_id,
                from_version=from_version,
                to_version=firmware_version,
                firmware_size=manifest.get('total_size', 0),
                chunks_total=manifest.get('total_chunks', 0),
                status='initiated'
            )
            
            # Update in-memory stats (for backwards compatibility)
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
        
        # Milestone 5: Inject chunk corruption if enabled (corrupt_chunk fault)
        if _should_inject_fault(device_id, chunk_index):
            if ota_fault_injection['fault_type'] == 'corrupt_chunk':
                chunk_data = _inject_chunk_corruption(chunk_data, chunk_index)
        
        # Update session
        import time
        with ota_lock:
            session['current_chunk'] = chunk_index
            session['bytes_transferred'] += len(chunk_data)
            session['last_activity'] = time.time()  # Use timestamp for easier comparison
            
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
        from database import Database
        
        with ota_lock:
            if device_id not in ota_sessions:
                logger.warning(f"No OTA session found for {device_id}")
                return False
            
            session = ota_sessions[device_id]
            session['status'] = 'completed' if success else 'failed'
            session['completed_at'] = datetime.now().isoformat()
            
            # Only update database status if success OR if current status is not already 'failed'
            # This prevents overwriting 'failed' status from verification_failed phase
            current_ota = Database.get_latest_ota_update(device_id)
            current_status = current_ota.get('status') if current_ota else None
            
            if success:
                # Success - always update to completed
                Database.update_ota_status(device_id=device_id, status='completed')
            elif current_status != 'failed':
                # Only update to failed if not already failed
                Database.update_ota_status(device_id=device_id, status='failed')
            else:
                # Already failed - don't overwrite
                logger.info(f"OTA status for {device_id} already 'failed', not overwriting")
            
            # Update in-memory stats (for backwards compatibility)
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
    Get OTA statistics from database
    
    Returns:
        dict: OTA statistics
    """
    try:
        from database import Database
        
        # Get statistics from database
        db_stats = Database.get_ota_statistics()
        
        logger.debug(f"Retrieved OTA stats from database: {db_stats}")
        return db_stats
        
    except Exception as e:
        logger.error(f"Error getting OTA stats: {e}")
        return {
            'total_updates_initiated': 0,
            'successful_updates': 0,
            'failed_updates': 0,
            'active_sessions': 0,
            'success_rate': 0.0,
            'error': str(e)
        }


def cancel_ota_session(device_id: str) -> bool:
    """
    Cancel an active OTA session
    
    Args:
        device_id: Device identifier
        
    Returns:
        bool: True if cancelled successfully
    """
    try:
        from database import Database
        
        with ota_lock:
            if device_id not in ota_sessions:
                logger.warning(f"No OTA session found for {device_id}")
                return False
            
            session = ota_sessions[device_id]
            session['status'] = 'cancelled'
            session['completed_at'] = datetime.now().isoformat()
            
            # Update database
            Database.update_ota_status(device_id=device_id, status='failed', error_msg='Cancelled by user')
            
            # Update in-memory stats (for backwards compatibility)
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
# FOTA FAULT INJECTION (For Testing - Milestone 5)
# ==============================================================================
# Supported fault types (triggers ESP32 rollback):
#   1. corrupt_chunk  - Corrupt firmware chunk data (CRC/hash validation fails)
#   2. bad_hash       - Incorrect SHA256 hash in manifest (verification fails)
#   3. bad_signature  - Incorrect signature in manifest (signature check fails)
# ==============================================================================

# Fault injection state
ota_fault_injection = {
    'enabled': False,
    'fault_type': None,           # 'corrupt_chunk', 'bad_hash', or 'bad_signature'
    'target_device': None,        # Target specific device or None for all
    'target_chunk': None,         # For corrupt_chunk: specific chunk or None for all
    'fault_count': 0,             # Number of faults injected
    'chunks_corrupted': []        # Track which chunks were corrupted
}

ota_fault_lock = threading.Lock()

# Valid OTA fault types (only these 3 for Milestone 5)
VALID_OTA_FAULT_TYPES = ['corrupt_chunk', 'bad_hash', 'bad_signature']


def enable_ota_fault_injection(fault_type: str, target_device: Optional[str] = None, 
                               target_chunk: Optional[int] = None, **kwargs) -> Tuple[bool, str]:
    """
    Enable OTA fault injection for testing (Milestone 5 requirement)
    
    Args:
        fault_type: Type of fault to inject (corrupt_chunk, bad_hash, bad_signature)
        target_device: Optional device ID to target (None = all devices)
        target_chunk: Optional chunk number to corrupt (None = all chunks, only for corrupt_chunk)
        
    Supported Fault Types (Milestone 5):
        - corrupt_chunk: Corrupt firmware chunk data by flipping random bits
                        ESP32 will detect CRC/hash mismatch and trigger rollback
        - bad_hash: Provide incorrect SHA256 hash in manifest
                   ESP32 will detect hash mismatch after download and trigger rollback
        - bad_signature: Provide incorrect signature in manifest
                        ESP32 will detect signature verification failure and trigger rollback
        
    Returns:
        tuple: (success, message)
    """
    with ota_fault_lock:
        if fault_type not in VALID_OTA_FAULT_TYPES:
            return False, f"Invalid fault type. Must be one of: {VALID_OTA_FAULT_TYPES}"
        
        ota_fault_injection['enabled'] = True
        ota_fault_injection['fault_type'] = fault_type
        ota_fault_injection['target_device'] = target_device
        ota_fault_injection['target_chunk'] = target_chunk
        ota_fault_injection['fault_count'] = 0
        ota_fault_injection['chunks_corrupted'] = []
        
        logger.warning(f"⚠️  OTA Fault Injection ENABLED: {fault_type}")
        if target_device:
            logger.warning(f"   Target Device: {target_device}")
        if target_chunk is not None and fault_type == 'corrupt_chunk':
            logger.warning(f"   Target Chunk: {target_chunk}")
        
        # Log expected ESP32 behavior
        if fault_type == 'corrupt_chunk':
            logger.warning("   → ESP32 will detect corrupted chunk data and trigger rollback")
        elif fault_type == 'bad_hash':
            logger.warning("   → ESP32 will detect SHA256 hash mismatch and trigger rollback")
        elif fault_type == 'bad_signature':
            logger.warning("   → ESP32 will detect signature verification failure and trigger rollback")
        
        return True, f"OTA fault injection enabled: {fault_type}"


def disable_ota_fault_injection() -> Tuple[bool, str]:
    """Disable OTA fault injection"""
    with ota_fault_lock:
        was_enabled = ota_fault_injection['enabled']
        fault_count = ota_fault_injection['fault_count']
        fault_type = ota_fault_injection['fault_type']
        chunks_corrupted = ota_fault_injection['chunks_corrupted'].copy()
        
        ota_fault_injection['enabled'] = False
        ota_fault_injection['fault_type'] = None
        ota_fault_injection['target_device'] = None
        ota_fault_injection['target_chunk'] = None
        ota_fault_injection['fault_count'] = 0
        ota_fault_injection['chunks_corrupted'] = []
        
        if was_enabled:
            msg = f"OTA fault injection disabled. Type: {fault_type}, Faults injected: {fault_count}"
            if chunks_corrupted:
                msg += f", Chunks corrupted: {chunks_corrupted}"
            logger.info(f"✓ {msg}")
            return True, msg
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
            'fault_count': ota_fault_injection['fault_count'],
            'chunks_corrupted': ota_fault_injection['chunks_corrupted'].copy(),
            'valid_fault_types': VALID_OTA_FAULT_TYPES
        }


def _should_inject_fault(device_id: str, chunk_number: Optional[int] = None) -> bool:
    """Check if fault should be injected for this request"""
    if not ota_fault_injection['enabled']:
        return False
    
    # Check device filter
    if ota_fault_injection['target_device'] and ota_fault_injection['target_device'] != device_id:
        return False
    
    # For corrupt_chunk, check chunk filter
    if ota_fault_injection['fault_type'] == 'corrupt_chunk' and chunk_number is not None:
        if ota_fault_injection['target_chunk'] is not None:
            if ota_fault_injection['target_chunk'] != chunk_number:
                return False
    
    return True


def _inject_chunk_corruption(chunk_data: bytes, chunk_index: int) -> bytes:
    """
    Corrupt chunk data for testing (Milestone 5: corrupt_chunk fault)
    
    Flips random bits in the chunk data to simulate data corruption.
    ESP32 will detect this via CRC check or hash verification and trigger rollback.
    
    Args:
        chunk_data: Raw chunk bytes
        chunk_index: Index of the chunk being corrupted
        
    Returns:
        Corrupted chunk bytes
    """
    import random
    
    ota_fault_injection['fault_count'] += 1
    ota_fault_injection['chunks_corrupted'].append(chunk_index)
    
    corrupted = bytearray(chunk_data)
    
    # Corrupt 5-10 random bytes by flipping bits
    num_corruptions = random.randint(5, 10)
    corruption_positions = []
    
    for _ in range(num_corruptions):
        if len(corrupted) > 0:
            pos = random.randint(0, len(corrupted) - 1)
            original_byte = corrupted[pos]
            corrupted[pos] ^= random.randint(1, 255)  # Flip random bits
            corruption_positions.append(pos)
    
    logger.warning(f"⚠️  CHUNK CORRUPTED: chunk {chunk_index} "
                  f"({num_corruptions} bytes modified at positions {corruption_positions[:5]}...) "
                  f"[fault #{ota_fault_injection['fault_count']}]")
    
    return bytes(corrupted)


def _inject_manifest_corruption(manifest: Dict) -> Dict:
    """
    Corrupt manifest data for testing (Milestone 5: bad_hash or bad_signature fault)
    
    Modifies the SHA256 hash or signature in the manifest to invalid values.
    ESP32 will detect this during verification and trigger rollback.
    
    Args:
        manifest: Update info/manifest dictionary
        
    Returns:
        Corrupted manifest dictionary
    """
    import random
    
    fault_type = ota_fault_injection['fault_type']
    ota_fault_injection['fault_count'] += 1
    
    if fault_type == 'bad_hash':
        # Corrupt SHA256 hash (64 hex characters)
        original_hash = manifest.get('sha256_hash', '')
        # Generate a completely different but valid-looking hash
        fake_hash = ''.join(random.choice('0123456789abcdef') for _ in range(64))
        manifest['sha256_hash'] = fake_hash
        
        logger.warning(f"⚠️  MANIFEST CORRUPTED: SHA256 hash replaced "
                      f"[fault #{ota_fault_injection['fault_count']}]")
        logger.warning(f"   Original hash: {original_hash[:16]}...{original_hash[-16:]}")
        logger.warning(f"   Fake hash:     {fake_hash[:16]}...{fake_hash[-16:]}")
        logger.warning(f"   → ESP32 will detect hash mismatch after download")
    
    elif fault_type == 'bad_signature':
        # Corrupt signature
        original_sig = manifest.get('signature', '')
        # Generate invalid signature
        fake_sig = 'INVALID_SIG_' + ''.join(random.choice('0123456789ABCDEF') for _ in range(32))
        manifest['signature'] = fake_sig
        
        logger.warning(f"⚠️  MANIFEST CORRUPTED: Signature replaced "
                      f"[fault #{ota_fault_injection['fault_count']}]")
        logger.warning(f"   Original signature: {original_sig[:20]}..." if original_sig else "   Original signature: (none)")
        logger.warning(f"   Fake signature:     {fake_sig[:20]}...")
        logger.warning(f"   → ESP32 will detect signature verification failure")
    
    return manifest


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
    'get_ota_fault_status',
    'VALID_OTA_FAULT_TYPES'
]

