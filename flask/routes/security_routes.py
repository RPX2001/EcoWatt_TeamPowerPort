"""
Security routes for EcoWatt Flask server
Handles security validation and statistics endpoints
"""

from flask import Blueprint, request, jsonify
import logging

from handlers import (
    validate_secured_payload,
    get_security_stats,
    reset_security_stats,
    clear_nonces,
    get_device_security_info
)
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
security_bp = Blueprint('security', __name__)


@security_bp.route('/security/validate/<device_id>', methods=['POST'])
def validate_payload(device_id: str):
    """Validate a secured payload from device"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        payload_json = request.get_json()
        
        # Convert to JSON string if needed
        if isinstance(payload_json, dict):
            import json
            payload_str = json.dumps(payload_json)
        else:
            payload_str = str(payload_json)
        
        success, decrypted, error = validate_secured_payload(device_id, payload_str)
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'decrypted_payload': decrypted
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': error or 'Validation failed'
            }), 401
        
    except Exception as e:
        logger.error(f"✗ Error validating payload for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@security_bp.route('/security/stats', methods=['GET'])
def get_security_statistics():
    """Get security statistics"""
    try:
        stats = get_security_stats()
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting security stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@security_bp.route('/security/stats', methods=['DELETE'])
def reset_security_statistics():
    """Reset security statistics"""
    try:
        success = reset_security_stats()
        
        if success:
            return jsonify({
                'success': True,
                'message': 'Security statistics reset'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to reset statistics'
            }), 500
        
    except Exception as e:
        logger.error(f"✗ Error resetting security stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@security_bp.route('/security/nonces/<device_id>', methods=['DELETE'])
def clear_device_nonces(device_id: str):
    """Clear nonces for a specific device"""
    try:
        success = clear_nonces(device_id)
        
        if success:
            return jsonify({
                'success': True,
                'message': f'Nonces cleared for {device_id}'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to clear nonces'
            }), 500
        
    except Exception as e:
        logger.error(f"✗ Error clearing nonces for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@security_bp.route('/security/nonces', methods=['DELETE'])
def clear_all_nonces():
    """Clear nonces for all devices"""
    try:
        success = clear_nonces(None)
        
        if success:
            return jsonify({
                'success': True,
                'message': 'All nonces cleared'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to clear nonces'
            }), 500
        
    except Exception as e:
        logger.error(f"✗ Error clearing all nonces: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@security_bp.route('/security/device/<device_id>', methods=['GET'])
def get_device_security(device_id: str):
    """Get security information for a specific device"""
    try:
        info = get_device_security_info(device_id)
        
        return jsonify({
            'success': True,
            'device_info': info
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting device security info: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
