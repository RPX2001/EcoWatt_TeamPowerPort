"""
OTA routes for EcoWatt Flask server
Handles firmware update endpoints
"""

from flask import Blueprint, request, jsonify
import logging
import time

from database import Database
from handlers import (
    check_for_update,
    initiate_ota_session,
    get_firmware_chunk_for_device,
    complete_ota_session,
    get_ota_status,
    get_ota_stats,
    cancel_ota_session,
    enable_ota_fault_injection,
    disable_ota_fault_injection,
    get_ota_fault_status
)
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
ota_bp = Blueprint('ota', __name__)


@ota_bp.route('/ota/check/<device_id>', methods=['GET'])
def check_update(device_id: str):
    """Check if firmware update is available for device"""
    try:
        current_version = request.args.get('version', default='1.0.0')
        
        update_available, update_info = check_for_update(device_id, current_version)
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'update_available': update_available,
            'update_info': update_info
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error checking update for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/initiate/<device_id>', methods=['POST'])
def initiate_update(device_id: str):
    """Initiate OTA update session for device"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        firmware_version = data.get('firmware_version')
        
        if not firmware_version:
            return jsonify({
                'success': False,
                'error': 'firmware_version is required'
            }), 400
        
        success, session_id_or_error = initiate_ota_session(device_id, firmware_version)
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'session_id': session_id_or_error
            }), 201
        else:
            return jsonify({
                'success': False,
                'error': session_id_or_error
            }), 400
        
    except Exception as e:
        logger.error(f"✗ Error initiating OTA for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/chunk/<device_id>', methods=['GET'])
def get_firmware_chunk(device_id: str):
    """Get firmware chunk for device"""
    try:
        firmware_version = request.args.get('version')
        chunk_index = request.args.get('chunk', type=int)
        
        if not firmware_version or chunk_index is None:
            return jsonify({
                'success': False,
                'error': 'version and chunk parameters are required'
            }), 400
        
        success, chunk_data, error = get_firmware_chunk_for_device(
            device_id, 
            firmware_version, 
            chunk_index
        )
        
        if success:
            # Encode chunk data as base64 for JSON transport
            import base64
            chunk_b64 = base64.b64encode(chunk_data).decode('utf-8')
            
            return jsonify({
                'success': True,
                'device_id': device_id,
                'chunk_index': chunk_index,
                'chunk_data': chunk_b64,
                'chunk_size': len(chunk_data)
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': error or 'Failed to get chunk'
            }), 400
        
    except Exception as e:
        logger.error(f"✗ Error getting firmware chunk for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/complete/<device_id>', methods=['POST'])
def complete_update(device_id: str):
    """Complete OTA update session"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        update_success = data.get('success', False)
        
        success = complete_ota_session(device_id, update_success)
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'message': 'OTA session completed'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to complete OTA session'
            }), 400
        
    except Exception as e:
        logger.error(f"✗ Error completing OTA for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/cancel/<device_id>', methods=['POST'])
def cancel_update(device_id: str):
    """Cancel active OTA session"""
    try:
        success = cancel_ota_session(device_id)
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'message': 'OTA session cancelled'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to cancel OTA session'
            }), 400
        
    except Exception as e:
        logger.error(f"✗ Error cancelling OTA for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/status/<device_id>', methods=['GET'])
def get_update_status(device_id: str):
    """Get OTA session status for device"""
    try:
        from database import Database
        from routes.device_routes import devices_registry
        from routes.config_routes import device_configs, get_default_config
        
        status = get_ota_status(device_id)
        
        # Get current firmware version from device registry or database
        device = Database.get_device(device_id)
        if device:
            status['current_firmware_version'] = device.get('firmware_version', 'unknown')
        elif device_id in devices_registry:
            status['current_firmware_version'] = devices_registry[device_id].get('firmware_version', 'unknown')
        else:
            status['current_firmware_version'] = 'unknown'
        
        # Get device-specific firmware_check_interval from config
        if device_id in device_configs:
            device_config = device_configs[device_id]
        else:
            device_config = get_default_config()
        
        firmware_check_interval_seconds = device_config.get('firmware_check_interval', 60)
        status['ota_check_interval_minutes'] = firmware_check_interval_seconds / 60
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'status': status
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting OTA status for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/status', methods=['GET'])
def get_all_ota_status():
    """Get OTA status for all devices"""
    try:
        status = get_ota_status(None)
        
        return jsonify({
            'success': True,
            'status': status
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting all OTA status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/stats', methods=['GET'])
def get_ota_statistics():
    """Get OTA statistics"""
    try:
        from database import Database
        
        stats = get_ota_stats()
        
        # Get firmware_check_interval from default config (in seconds)
        # Convert to minutes for display
        from routes.config_routes import get_default_config
        default_config = get_default_config()
        firmware_check_interval_seconds = default_config.get('firmware_check_interval', 60)
        stats['ota_check_interval_minutes'] = firmware_check_interval_seconds / 60
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting OTA stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


# ==============================================================================
# FOTA FAULT INJECTION ENDPOINTS (For Testing)
# ==============================================================================

@ota_bp.route('/ota/test/enable', methods=['POST'])
def enable_fault_injection():
    """Enable OTA fault injection for testing"""
    try:
        data = request.get_json()
        
        fault_type = data.get('fault_type')
        target_device = data.get('target_device')
        target_chunk = data.get('target_chunk')
        
        if not fault_type:
            return jsonify({
                'success': False,
                'error': 'fault_type is required'
            }), 400
        
        success, message = enable_ota_fault_injection(fault_type, target_device, target_chunk)
        
        if success:
            return jsonify({
                'success': True,
                'message': message,
                'fault_config': {
                    'fault_type': fault_type,
                    'target_device': target_device,
                    'target_chunk': target_chunk
                }
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': message
            }), 400
            
    except Exception as e:
        logger.error(f"✗ Error enabling fault injection: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/test/disable', methods=['POST'])
def disable_fault_injection():
    """Disable OTA fault injection"""
    try:
        success, message = disable_ota_fault_injection()
        
        return jsonify({
            'success': True,
            'message': message
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error disabling fault injection: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/test/status', methods=['GET'])
def get_fault_injection_status():
    """Get current fault injection status"""
    try:
        status = get_ota_fault_status()
        
        return jsonify({
            'success': True,
            'fault_injection': status
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting fault injection status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/upload', methods=['POST'])
def upload_firmware():
    """
    Upload a new firmware binary
    
    Expected form data:
        file: Firmware binary file
        version: Firmware version (e.g., "1.0.5")
        description: Optional description
        
    Returns:
        JSON with upload status and firmware info
    """
    try:
        # Check if file is present
        if 'file' not in request.files:
            return jsonify({
                'success': False,
                'error': 'No file provided'
            }), 400
        
        file = request.files['file']
        if file.filename == '':
            return jsonify({
                'success': False,
                'error': 'Empty filename'
            }), 400
        
        version = request.form.get('version')
        if not version:
            return jsonify({
                'success': False,
                'error': 'Version is required'
            }), 400
        
        description = request.form.get('description', '')
        
        # Save firmware file temporarily
        import os
        import tempfile
        firmware_dir = 'firmware'
        os.makedirs(firmware_dir, exist_ok=True)
        
        # Check if version already exists
        encrypted_filename = f"firmware_{version}_encrypted.bin"
        encrypted_path = os.path.join(firmware_dir, encrypted_filename)
        if os.path.exists(encrypted_path):
            return jsonify({
                'success': False,
                'error': f'Firmware version {version} already exists'
            }), 409
        
        # Save original binary temporarily
        temp_firmware_path = os.path.join(firmware_dir, f"firmware_{version}_temp.bin")
        file.save(temp_firmware_path)
        
        try:
            # Use FirmwareManager to prepare firmware (encrypt, sign, chunk)
            import sys
            from pathlib import Path
            
            # Add scripts directory to path
            scripts_dir = Path(__file__).parent.parent / 'scripts'
            if str(scripts_dir) not in sys.path:
                sys.path.insert(0, str(scripts_dir))
            
            from firmware_manager import FirmwareManager
            
            fm = FirmwareManager(firmware_dir=firmware_dir, keys_dir='keys')
            
            logger.info(f"Preparing firmware {version} for OTA...")
            manifest = fm.prepare_firmware(temp_firmware_path, version)
            
            # Add backward-compatible fields for frontend
            import time
            manifest['size'] = manifest['encrypted_size']  # Frontend expects 'size'
            manifest['uploaded_at'] = time.time()  # Numeric timestamp for sorting
            manifest['status'] = 'ready'  # Add status field
            
            # Add description to manifest
            if description:
                manifest['description'] = description
            
            # Save updated manifest with all fields
            manifest_path = os.path.join(firmware_dir, f"firmware_{version}_manifest.json")
            import json
            with open(manifest_path, 'w') as f:
                json.dump(manifest, f, indent=2)
            
            # Clean up temporary file
            os.remove(temp_firmware_path)
            
            logger.info(f"Firmware uploaded and prepared: {version} ({manifest['encrypted_size']} bytes encrypted)")
            
            return jsonify({
                'success': True,
                'message': f'Firmware {version} uploaded and prepared successfully',
                'firmware': {
                    'version': manifest['version'],
                    'original_size': manifest['original_size'],
                    'encrypted_size': manifest['encrypted_size'],
                    'total_chunks': manifest['total_chunks'],
                    'chunk_size': manifest['chunk_size'],
                    'sha256_hash': manifest['sha256_hash'],
                    'uploaded_at': manifest['timestamp'],
                    'description': description
                }
            }), 201
            
        except Exception as e:
            # Clean up temporary file on error
            if os.path.exists(temp_firmware_path):
                os.remove(temp_firmware_path)
            raise e
        
    except Exception as e:
        logger.error(f"✗ Error uploading firmware: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/firmwares', methods=['GET'])
def list_firmwares():
    """
    List all available firmware versions
    
    Returns:
        JSON with list of firmware versions
    """
    try:
        import os
        import json
        
        firmware_dir = 'firmware'
        if not os.path.exists(firmware_dir):
            return jsonify({
                'success': True,
                'firmwares': []
            }), 200
        
        firmwares = []
        for filename in os.listdir(firmware_dir):
            if filename.endswith('_manifest.json'):
                manifest_path = os.path.join(firmware_dir, filename)
                with open(manifest_path, 'r') as f:
                    manifest = json.load(f)
                    
                    # Ensure uploaded_at is numeric for sorting
                    if 'uploaded_at' not in manifest or not isinstance(manifest.get('uploaded_at'), (int, float)):
                        # Try to use timestamp if uploaded_at is missing or non-numeric
                        import time
                        manifest['uploaded_at'] = time.time()
                    
                    # Ensure size field exists (backward compatibility)
                    if 'size' not in manifest and 'encrypted_size' in manifest:
                        manifest['size'] = manifest['encrypted_size']
                    
                    firmwares.append(manifest)
        
        # Sort by upload time, newest first
        firmwares.sort(key=lambda x: x.get('uploaded_at', 0), reverse=True)
        
        return jsonify({
            'success': True,
            'firmwares': firmwares,
            'count': len(firmwares)
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error listing firmwares: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/firmware/<version>/manifest', methods=['GET'])
def get_firmware_manifest_endpoint(version):
    """
    Get firmware manifest for specific version
    
    Returns:
        JSON with firmware manifest
    """
    try:
        import os
        import json
        
        firmware_dir = 'firmware'
        manifest_path = os.path.join(firmware_dir, f"firmware_{version}_manifest.json")
        
        if not os.path.exists(manifest_path):
            return jsonify({
                'success': False,
                'error': f'Firmware version {version} not found'
            }), 404
        
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
        
        return jsonify({
            'success': True,
            'manifest': manifest
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting firmware manifest for {version}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/firmware/<version>', methods=['DELETE'])
def delete_firmware_endpoint(version):
    """
    Delete firmware version
    
    Returns:
        JSON with deletion status
    """
    try:
        import os
        
        firmware_dir = 'firmware'
        firmware_path = os.path.join(firmware_dir, f"firmware_{version}.bin")
        manifest_path = os.path.join(firmware_dir, f"firmware_{version}_manifest.json")
        
        if not os.path.exists(firmware_path) and not os.path.exists(manifest_path):
            return jsonify({
                'success': False,
                'error': f'Firmware version {version} not found'
            }), 404
        
        # Delete firmware binary
        if os.path.exists(firmware_path):
            os.remove(firmware_path)
            logger.info(f"Deleted firmware binary: {firmware_path}")
        
        # Delete manifest
        if os.path.exists(manifest_path):
            os.remove(manifest_path)
            logger.info(f"Deleted firmware manifest: {manifest_path}")
        
        return jsonify({
            'success': True,
            'message': f'Firmware version {version} deleted successfully'
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error deleting firmware {version}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/<device_id>/progress', methods=['POST'])
def ota_progress_report(device_id):
    """
    Receive OTA progress updates from ESP32 during download/verification
    
    Expected JSON body:
    {
        "phase": "downloading" | "download_complete" | "verifying" | "verification_success" | "verification_failed" | "installing" | "rollback",
        "progress": 0-100,  # Download progress percentage
        "message": "Human-readable status message",
        "timestamp": 1234567890  # Unix timestamp
    }
    """
    try:
        from handlers.ota_handler import ota_sessions, ota_lock
        
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        # Validate required fields
        if 'phase' not in data:
            return jsonify({
                'success': False,
                'error': 'Missing required field: phase'
            }), 400
        
        phase = data['phase']
        progress = data.get('progress', 0)
        message = data.get('message', '')
        
        # Update OTA session with progress info
        with ota_lock:
            if device_id in ota_sessions:
                ota_sessions[device_id]['phase'] = phase
                ota_sessions[device_id]['progress'] = progress
                ota_sessions[device_id]['message'] = message
                ota_sessions[device_id]['last_activity'] = time.time()
        
        # Map phase to database status and update database
        status_mapping = {
            'downloading': 'downloading',
            'download_complete': 'downloaded',
            'verifying': 'verifying',
            'verification_success': 'verified',
            'verification_failed': 'failed',
            'installing': 'installing',
            'rollback': 'rollback'
        }
        
        db_status = status_mapping.get(phase, phase)
        
        # Update database status based on phase
        if phase in ['downloading', 'download_complete']:
            # Calculate chunks downloaded from progress percentage
            if device_id in ota_sessions:
                total_chunks = ota_sessions[device_id].get('total_chunks', 0)
                chunks_downloaded = int((progress / 100.0) * total_chunks) if total_chunks > 0 else None
                Database.update_ota_download_status(device_id, db_status, chunks_downloaded, None if phase != 'verification_failed' else message)
        elif phase in ['verification_failed', 'rollback']:
            Database.update_ota_status(device_id, 'failed', message)
        elif phase in ['installing', 'verification_success']:
            Database.update_ota_status(device_id, db_status, None)
        
        logger.info(f"[OTA Progress] {device_id}: {phase} - {progress}% - {message}")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'message': 'Progress updated'
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error recording OTA progress for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/<device_id>/complete', methods=['POST'])
def ota_complete(device_id):
    """
    Receive OTA completion status from ESP32 after reboot
    
    Expected JSON body:
    {
        "version": "1.0.5",  # New version after update
        "status": "success" | "failed" | "rolled_back",
        "timestamp": 1234567890,  # Unix timestamp
        "error_msg": "Optional error message if failed",
        "download_status": "success" | "failed"  # Optional: status before reboot
    }
    """
    try:
        from database import Database
        
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        # Validate required fields
        if 'version' not in data or 'status' not in data:
            return jsonify({
                'success': False,
                'error': 'Missing required fields: version, status'
            }), 400
        
        version = data['version']
        status = data['status']
        error_msg = data.get('error_msg')
        
        # Always clean up OTA session first (even if no DB record exists)
        from handlers.ota_handler import complete_ota_session
        is_success = (status == 'success')
        complete_ota_session(device_id, is_success)
        
        logger.info(f"[OTA] ESP32 reported OTA completion for {device_id}: v{version} - {status}")
        
        # Try to update database with installation status (optional - may not exist)
        db_success = Database.update_ota_install_status(
            device_id=device_id,
            to_version=version,
            status=status,
            error_msg=error_msg
        )
        
        if not db_success:
            logger.warning(f"[!] [OTA] No OTA record found in database for {device_id} version {version} (session still cleaned up)")
        
        # Also update device firmware version in database if successful
        if is_success:
            Database.update_device_firmware(device_id, version)
            logger.info(f"[OTA] Updated device {device_id} firmware version to {version}")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'message': 'OTA completion status received',
            'version': version,
            'status': status
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error recording OTA completion for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
