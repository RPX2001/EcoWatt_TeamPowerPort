"""
OTA routes for EcoWatt Flask server
Handles firmware update endpoints
"""

from flask import Blueprint, request, jsonify
import logging

from handlers import (
    check_for_update,
    initiate_ota_session,
    get_firmware_chunk_for_device,
    complete_ota_session,
    get_ota_status,
    get_ota_stats,
    cancel_ota_session
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
