"""
Command Management Routes
Handles command queuing and execution following Milestone 4 specification
Based on: Milestone 4 - Command Execution Protocol Specification

Cloud → Device Command Message:
{
  "command": {
    "action": "write_register",
    "target_register": "export_power",
    "value": 50
  }
}

Device → Cloud Execution Result:
{
  "command_result": {
    "status": "success",
    "executed_at": "2025-09-04T14:12:00Z"
  }
}

Commands are queued by cloud and executed by device at next scheduled communication window.
Device must forward command to Inverter SIM and return its result.
"""

from flask import Blueprint, jsonify, request
import logging
import time
import uuid
from typing import Dict, List

# Import command handler functions
from handlers.command_handler import (
    queue_command as handler_queue_command,
    poll_commands as handler_poll_commands,
    submit_command_result as handler_submit_result,
    get_command_status as handler_get_status,
    get_command_history as handler_get_history,
    get_command_stats as handler_get_stats,  # NOTE: returns dict directly, not tuple
    reset_command_stats as handler_reset_stats,
    command_queues,  # For test access
    command_history,  # For test access
    command_stats     # For test access
)

logger = logging.getLogger(__name__)

# Create blueprint
command_bp = Blueprint('command', __name__)


@command_bp.route('/commands/<device_id>', methods=['POST'])
def queue_command_route(device_id):
    """
    Queue a command for execution
    
    STRICT Milestone 4 format:
    Request: {
      "command": {
        "action": "write_register",
        "target_register": "export_power",
        "value": 50
      }
    }
    Response: { "success": true, "command_id": "uuid", "note": "..." }
    
    Commands are queued and executed at next communication window.
    """
    try:
        data = request.get_json()
        
        if not data or 'command' not in data:
            return jsonify({
                'success': False,
                'error': 'command field is required'
            }), 400
        
        command_data = data['command']
        if not isinstance(command_data, dict):
            return jsonify({
                'success': False,
                'error': 'command must be an object with action, target_register, and value'
            }), 400
        
        if 'action' not in command_data:
            return jsonify({
                'success': False,
                'error': 'command.action is required'
            }), 400
        
        # Extract M4 format fields
        action = command_data['action']
        target_register = command_data.get('target_register')
        value = command_data.get('value')
        
        # Use handler to queue command (store full command object)
        success, result = handler_queue_command(
            device_id=device_id,
            command=action,
            parameters={
                'target_register': target_register,
                'value': value
            }
        )
        
        if not success:
            return jsonify({
                'success': False,
                'error': result
            }), 500
        
        command_id = result
        
        logger.info(f"[Command] Queued command {command_id} for device {device_id}: {action}")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'command_id': command_id,
            'note': 'Command queued. Will be executed at next scheduled communication window.'
        }), 201
        
    except Exception as e:
        logger.error(f"Error queuing command for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/poll', methods=['GET'])
def poll_commands_route(device_id):
    """
    Poll for pending commands - SINGLE STANDARD ENDPOINT
    
    Used by ESP32 device to poll for commands at configured interval
    Frontend and tests use this same endpoint
    
    Query params:
    - limit: max commands to return (default: 10)
    
    Response: { "success": true, "commands": [...], "count": n }
    """
    try:
        limit = int(request.args.get('limit', 10))
        
        # Use handler to poll commands
        success, commands = handler_poll_commands(device_id, limit)
        
        if not success:
            return jsonify({
                'success': False,
                'error': 'Failed to poll commands'
            }), 500
        
        logger.info(f"[Command] Device {device_id} polled, {len(commands)} commands returned")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'commands': commands,
            'count': len(commands)
        }), 200
        
    except Exception as e:
        logger.error(f"Error polling commands for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/result', methods=['POST'])
def submit_command_result_route(device_id):
    """
    Submit command execution result
    
    STRICT Milestone 4 format:
    Request: {
      "command_result": {
        "command_id": "uuid",
        "status": "success",
        "executed_at": "2025-09-04T14:12:00Z"
      }
    }
    
    Device forwards command to Inverter SIM and returns result.
    """
    try:
        data = request.get_json()
        
        if not data or 'command_result' not in data:
            return jsonify({
                'success': False,
                'error': 'command_result field is required'
            }), 400
        
        result = data['command_result']
        command_id = result.get('command_id')
        
        if not command_id:
            return jsonify({
                'success': False,
                'error': 'command_result.command_id is required'
            }), 400
        
        # Extract M4 format fields
        status = result.get('status', 'unknown')
        success = (status == 'success')
        executed_at = result.get('executed_at')
        
        # Use handler to submit result
        submitted = handler_submit_result(
            device_id=device_id,
            command_id=command_id,
            success=success,
            result_data={'executed_at': executed_at} if executed_at else None,
            error_message=result.get('error')
        )
        
        if not submitted:
            return jsonify({
                'success': False,
                'error': f'Command {command_id} not found'
            }), 404
        
        logger.info(f"[Command] Received result for command {command_id}: {status}")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'command_id': command_id,
            'status': 'completed' if success else 'failed'
        }), 200
        
    except Exception as e:
        logger.error(f"Error submitting command result: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/status/<command_id>', methods=['GET'])
def get_command_status_route(command_id):
    """
    Get command execution status
    
    Returns current status of a command (pending, executing, success, failed)
    """
    try:
        # Use handler to get status
        success, status_info = handler_get_status(command_id)
        
        if not success:
            return jsonify({
                'success': False,
                'error': f'Command {command_id} not found'
            }), 404
        
        return jsonify({
            'success': True,
            'command_id': command_id,
            'status': status_info  # Nested status object as tests expect
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting command status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
    except Exception as e:
        logger.error(f"Error getting command status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/history', methods=['GET'])
def get_command_history_route(device_id):
    """
    Get command execution history for a device
    
    Query parameters:
        limit: Number of history entries to return (default: 20, max: 100)
    """
    try:
        limit = int(request.args.get('limit', 20))
        limit = min(limit, 100)  # Cap at 100
        
        # Use handler to get history
        success, history = handler_get_history(device_id, limit)
        
        if not success:
            return jsonify({
                'success': False,
                'error': 'Failed to get history'
            }), 500
        
        logger.info(f"[Command] History requested for {device_id}, returning {len(history)} commands")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'commands': history,
            'count': len(history)
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting command history for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/stats', methods=['GET'])
def get_command_statistics_route():
    """
    Get overall command statistics
    
    Returns aggregate statistics across all devices
    """
    try:
        # Use handler to get statistics (returns dict directly)
        stats = handler_get_stats()
        
        if 'error' in stats:
            return jsonify({
                'success': False,
                'error': stats['error']
            }), 500
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting command statistics: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/stats', methods=['DELETE'])
def reset_command_statistics_route():
    """
    Reset command statistics and clear history
    """
    try:
        # Use handler to reset statistics
        handler_reset_stats()
        
        logger.info("[Command] Command statistics and history reset")
        
        return jsonify({
            'success': True,
            'message': 'Command statistics reset'
        }), 200
        
    except Exception as e:
        logger.error(f"Error resetting command statistics: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
