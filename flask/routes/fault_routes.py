"""
Fault injection routes for EcoWatt Flask server
Handles fault injection testing endpoints
"""

from flask import Blueprint, request, jsonify
import logging

from handlers import (
    enable_fault_injection,
    disable_fault_injection,
    get_fault_status,
    get_available_faults,
    get_fault_statistics,
    reset_fault_statistics
)

logger = logging.getLogger(__name__)

# Create blueprint
fault_bp = Blueprint('fault', __name__)


@fault_bp.route('/fault/enable', methods=['POST'])
def enable_fault():
    """Enable a specific fault injection"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        fault_type = data.get('fault_type')
        parameters = data.get('parameters', {})
        
        if not fault_type:
            return jsonify({
                'success': False,
                'error': 'fault_type is required'
            }), 400
        
        success, error = enable_fault_injection(fault_type, parameters)
        
        if success:
            return jsonify({
                'success': True,
                'fault_type': fault_type,
                'message': f'Fault injection enabled: {fault_type}'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': error or 'Failed to enable fault injection'
            }), 400
        
    except Exception as e:
        logger.error(f"Error enabling fault injection: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/disable', methods=['POST'])
def disable_fault():
    """Disable fault injection"""
    try:
        # Get fault_type from query params or JSON body
        fault_type = request.args.get('fault_type')
        
        if not fault_type and request.is_json:
            data = request.get_json()
            fault_type = data.get('fault_type')
        
        success = disable_fault_injection(fault_type)
        
        if success:
            message = f'Fault injection disabled: {fault_type}' if fault_type else 'All fault injections disabled'
            return jsonify({
                'success': True,
                'message': message
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to disable fault injection'
            }), 400
        
    except Exception as e:
        logger.error(f"Error disabling fault injection: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/status', methods=['GET'])
def get_status():
    """Get current fault injection status"""
    try:
        status = get_fault_status()
        
        return jsonify({
            'success': True,
            'status': status
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting fault status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/available', methods=['GET'])
def get_available():
    """Get list of available fault types"""
    try:
        faults = get_available_faults()
        
        return jsonify({
            'success': True,
            'faults': faults
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting available faults: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/stats', methods=['GET'])
def get_stats():
    """Get fault injection statistics"""
    try:
        stats = get_fault_statistics()
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting fault stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/stats', methods=['DELETE'])
def reset_stats():
    """Reset fault injection statistics"""
    try:
        success = reset_fault_statistics()
        
        if success:
            return jsonify({
                'success': True,
                'message': 'Fault statistics reset'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to reset statistics'
            }), 500
        
    except Exception as e:
        logger.error(f"Error resetting fault stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
