"""
OTA routes for EcoWatt Flask server
Handles firmware update endpoints
"""

from flask import Blueprint, request, jsonify
import logging
import time

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
        logger.error(f"Error checking update for {device_id}: {e}")
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
        logger.error(f"Error initiating OTA for {device_id}: {e}")
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
        logger.error(f"Error getting firmware chunk for {device_id}: {e}")
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
        logger.error(f"Error completing OTA for {device_id}: {e}")
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
        logger.error(f"Error cancelling OTA for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/status/<device_id>', methods=['GET'])
def get_update_status(device_id: str):
    """Get OTA session status for device"""
    try:
        status = get_ota_status(device_id)
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'status': status
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting OTA status for {device_id}: {e}")
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
        logger.error(f"Error getting all OTA status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@ota_bp.route('/ota/stats', methods=['GET'])
def get_ota_statistics():
    """Get OTA statistics"""
    try:
        stats = get_ota_stats()
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting OTA stats: {e}")
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
        logger.error(f"Error enabling fault injection: {e}")
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
        logger.error(f"Error disabling fault injection: {e}")
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
        logger.error(f"Error getting fault injection status: {e}")
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
        
        # Save firmware file
        import os
        firmware_dir = 'firmware'
        os.makedirs(firmware_dir, exist_ok=True)
        
        filename = f"firmware_{version}.bin"
        filepath = os.path.join(firmware_dir, filename)
        
        # Check if version already exists
        if os.path.exists(filepath):
            return jsonify({
                'success': False,
                'error': f'Firmware version {version} already exists'
            }), 409
        
        file.save(filepath)
        
        # Get file size
        file_size = os.path.getsize(filepath)
        
        # Create manifest
        import json
        import time
        import hashlib
        
        # Calculate SHA256 hash
        with open(filepath, 'rb') as f:
            firmware_hash = hashlib.sha256(f.read()).hexdigest()
        
        manifest = {
            'version': version,
            'filename': filename,
            'size': file_size,
            'hash': firmware_hash,
            'description': description,
            'uploaded_at': time.time()
        }
        
        manifest_path = os.path.join(firmware_dir, f"firmware_{version}_manifest.json")
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
        
        logger.info(f"Firmware uploaded: {version} ({file_size} bytes)")
        
        return jsonify({
            'success': True,
            'message': f'Firmware {version} uploaded successfully',
            'firmware': manifest
        }), 201
        
    except Exception as e:
        logger.error(f"Error uploading firmware: {e}")
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
                    firmwares.append(manifest)
        
        # Sort by upload time, newest first
        firmwares.sort(key=lambda x: x.get('uploaded_at', 0), reverse=True)
        
        return jsonify({
            'success': True,
            'firmwares': firmwares,
            'count': len(firmwares)
        }), 200
        
    except Exception as e:
        logger.error(f"Error listing firmwares: {e}")
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
        
        # Update database with installation status
        success = Database.update_ota_install_status(
            device_id=device_id,
            to_version=version,
            status=status,
            error_msg=error_msg
        )
        
        if success:
            logger.info(f"[OTA] ESP32 reported OTA completion for {device_id}: v{version} - {status}")
            
            # Also update OTA session status in memory (for compatibility)
            from handlers.ota_handler import ota_sessions
            if device_id in ota_sessions:
                ota_sessions[device_id]['status'] = status
                ota_sessions[device_id]['completed_at'] = time.time()
            
            return jsonify({
                'success': True,
                'device_id': device_id,
                'message': 'OTA completion status received',
                'version': version,
                'status': status
            }), 200
        else:
            logger.warning(f"[OTA] No OTA record found for {device_id} version {version}")
            return jsonify({
                'success': False,
                'error': 'No matching OTA record found'
            }), 404
        
    except Exception as e:
        logger.error(f"Error recording OTA completion for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
