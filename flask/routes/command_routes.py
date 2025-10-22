"""
Command routes for EcoWatt Flask server
Handles command queuing and execution endpoints
"""

from flask import Blueprint, request, jsonify
import logging

from handlers import (
    queue_command,
    poll_commands,
    submit_command_result,
    get_command_status,
    get_command_stats,
    reset_command_stats
)

logger = logging.getLogger(__name__)

# Create blueprint
command_bp = Blueprint('command', __name__)


@command_bp.route('/commands/<device_id>', methods=['POST'])
def queue_device_command(device_id: str):
    """Queue a command for device"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        command = data.get('command')
        parameters = data.get('parameters', {})
        
        if not command:
            return jsonify({
                'success': False,
                'error': 'command field is required'
            }), 400
        
        success, command_id_or_error = queue_command(device_id, command, parameters)
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'command_id': command_id_or_error
            }), 201
        else:
            return jsonify({
                'success': False,
                'error': command_id_or_error
            }), 400
        
    except Exception as e:
        logger.error(f"Error queuing command for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/poll', methods=['GET'])
def poll_device_commands(device_id: str):
    """Poll pending commands for device"""
    try:
        limit = request.args.get('limit', default=10, type=int)
        
        success, commands = poll_commands(device_id, limit)
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'commands': commands
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to poll commands'
            }), 500
        
    except Exception as e:
        logger.error(f"Error polling commands for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/result', methods=['POST'])
def submit_result(device_id: str):
    """Submit command execution result"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        command_id = data.get('command_id')
        result_success = data.get('success', False)
        result_data = data.get('result_data', {})
        error_message = data.get('error_message')
        
        if not command_id:
            return jsonify({
                'success': False,
                'error': 'command_id is required'
            }), 400
        
        success = submit_command_result(
            device_id,
            command_id,
            result_success,
            result_data,
            error_message
        )
        
        if success:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'command_id': command_id
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to submit result'
            }), 400
        
    except Exception as e:
        logger.error(f"Error submitting result for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/status/<command_id>', methods=['GET'])
def get_command_status_route(command_id: str):
    """Get command execution status"""
    try:
        success, status_info = get_command_status(command_id)
        
        if success:
            return jsonify({
                'success': True,
                'command_id': command_id,
                'status': status_info
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Command not found'
            }), 404
        
    except Exception as e:
        logger.error(f"Error getting command status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/stats', methods=['GET'])
def get_command_statistics():
    """Get command statistics"""
    try:
        stats = get_command_stats()
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting command stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/stats', methods=['DELETE'])
def reset_command_statistics():
    """Reset command statistics"""
    try:
        success = reset_command_stats()
        
        if success:
            return jsonify({
                'success': True,
                'message': 'Command statistics reset'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to reset statistics'
            }), 500
        
    except Exception as e:
        logger.error(f"Error resetting command stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
