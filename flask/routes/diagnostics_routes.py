"""
Diagnostics routes for EcoWatt Flask server
Handles diagnostic data storage and retrieval endpoints
"""

from flask import Blueprint, request, jsonify
import logging

from handlers import (
    store_diagnostics,
    get_diagnostics_by_device,
    get_all_diagnostics,
    clear_diagnostics,
    get_diagnostics_summary
)

logger = logging.getLogger(__name__)

# Create blueprint
diagnostics_bp = Blueprint('diagnostics', __name__)


@diagnostics_bp.route('/diagnostics', methods=['GET'])
def get_all_diagnostics_route():
    """Get diagnostics for all devices"""
    try:
        limit = request.args.get('limit', default=10, type=int)
        diagnostics = get_all_diagnostics(limit_per_device=limit)
        
        return jsonify({
            'success': True,
            'diagnostics': diagnostics
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting all diagnostics: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@diagnostics_bp.route('/diagnostics/<device_id>', methods=['GET'])
def get_device_diagnostics(device_id: str):
    """Get diagnostics for a specific device"""
    try:
        limit = request.args.get('limit', default=10, type=int)
        diagnostics = get_diagnostics_by_device(device_id, limit=limit)
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'diagnostics': diagnostics
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting diagnostics for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@diagnostics_bp.route('/diagnostics/<device_id>', methods=['POST'])
def store_device_diagnostics(device_id: str):
    """Store diagnostic data for a device"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        diagnostics_data = request.get_json()
        
        success = store_diagnostics(device_id, diagnostics_data)
        
        if success:
            return jsonify({
                'success': True,
                'message': f'Diagnostics stored for {device_id}'
            }), 201
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to store diagnostics'
            }), 500
        
    except Exception as e:
        logger.error(f"Error storing diagnostics for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@diagnostics_bp.route('/diagnostics/<device_id>', methods=['DELETE'])
def clear_device_diagnostics(device_id: str):
    """Clear diagnostics for a specific device"""
    try:
        success = clear_diagnostics(device_id)
        
        if success:
            return jsonify({
                'success': True,
                'message': f'Diagnostics cleared for {device_id}'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to clear diagnostics'
            }), 500
        
    except Exception as e:
        logger.error(f"Error clearing diagnostics for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@diagnostics_bp.route('/diagnostics/summary', methods=['GET'])
def get_diagnostics_summary_route():
    """Get summary statistics for diagnostics"""
    try:
        device_id = request.args.get('device_id', default=None)
        summary = get_diagnostics_summary(device_id)
        
        return jsonify({
            'success': True,
            'summary': summary
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting diagnostics summary: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@diagnostics_bp.route('/diagnostics', methods=['DELETE'])
def clear_all_diagnostics():
    """Clear diagnostics for all devices"""
    try:
        success = clear_diagnostics(None)
        
        if success:
            return jsonify({
                'success': True,
                'message': 'All diagnostics cleared'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to clear diagnostics'
            }), 500
        
    except Exception as e:
        logger.error(f"Error clearing all diagnostics: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
