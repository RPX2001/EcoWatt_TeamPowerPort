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

logger = logging.getLogger(__name__)

# Create blueprint
command_bp = Blueprint('command', __name__)

# In-memory command storage (in production, use database)
command_queue: Dict[str, list] = {}  # device_id -> list of pending commands
command_history: Dict[str, list] = {}  # device_id -> list of executed commands
command_status: Dict[str, dict] = {}  # command_id -> command details


@command_bp.route('/commands/<device_id>', methods=['POST'])
def queue_command(device_id):
    """
    Queue a command for execution
    
    Following Milestone 4 format:
    Request: { "action": "write_register", "register_address": 8, "register_value": 50 }
    Response: { "success": true, "command_id": "uuid", "note": "..." }
    
    Commands are queued and executed at next communication window.
    """
    try:
        data = request.get_json()
        
        if not data or 'action' not in data:
            return jsonify({
                'success': False,
                'error': 'Missing action in request body'
            }), 400
        
        # Generate command ID
        command_id = str(uuid.uuid4())
        
        # Create command object
        command = {
            'command_id': command_id,
            'device_id': device_id,
            'action': data['action'],
            'parameters': {k: v for k, v in data.items() if k != 'action'},
            'status': 'pending',
            'queued_at': time.time(),
            'executed_at': None,
            'result': None
        }
        
        # Add to queue
        if device_id not in command_queue:
            command_queue[device_id] = []
        
        command_queue[device_id].append(command)
        
        # Track in status dict
        command_status[command_id] = command
        
        logger.info(f"[Command] Queued command {command_id} for device {device_id}: {data['action']}")
        
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


@command_bp.route('/commands/<device_id>/pending', methods=['GET'])
def get_pending_commands(device_id):
    """
    Get pending commands for a device
    
    Used by device to poll for commands at configured interval (command_poll_interval)
    """
    try:
        limit = int(request.args.get('limit', 10))
        
        if device_id not in command_queue:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'commands': [],
                'count': 0
            }), 200
        
        pending_commands = [
            cmd for cmd in command_queue[device_id]
            if cmd['status'] == 'pending'
        ][:limit]
        
        logger.info(f"[Command] Device {device_id} polled, {len(pending_commands)} pending commands")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'commands': pending_commands,
            'count': len(pending_commands)
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting pending commands for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/result', methods=['POST'])
def submit_command_result(device_id):
    """
    Submit command execution result
    
    Following Milestone 4 format:
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
                'error': 'Missing command_result in request body'
            }), 400
        
        result = data['command_result']
        command_id = result.get('command_id')
        
        if not command_id:
            return jsonify({
                'success': False,
                'error': 'Missing command_id in command_result'
            }), 400
        
        # Find command
        if command_id not in command_status:
            return jsonify({
                'success': False,
                'error': f'Command {command_id} not found'
            }), 404
        
        command = command_status[command_id]
        
        # Update command status
        command['status'] = result.get('status', 'unknown')
        command['executed_at'] = result.get('executed_at', time.time())
        command['result'] = result
        
        # Move from queue to history
        if device_id in command_queue:
            command_queue[device_id] = [
                cmd for cmd in command_queue[device_id]
                if cmd['command_id'] != command_id
            ]
        
        if device_id not in command_history:
            command_history[device_id] = []
        
        command_history[device_id].append(command)
        
        # Keep only last 100 commands in history
        if len(command_history[device_id]) > 100:
            command_history[device_id] = command_history[device_id][-100:]
        
        logger.info(f"[Command] Received result for command {command_id}: {command['status']}")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'command_id': command_id,
            'status': command['status']
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
        if command_id not in command_status:
            return jsonify({
                'success': False,
                'error': f'Command {command_id} not found'
            }), 404
        
        command = command_status[command_id]
        
        return jsonify({
            'success': True,
            'command_id': command_id,
            'status': command['status'],
            'command': command
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting command status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/<device_id>/history', methods=['GET'])
def get_command_history(device_id):
    """
    Get command execution history for a device
    
    Query parameters:
        limit: Number of history entries to return (default: 20, max: 100)
    """
    try:
        limit = int(request.args.get('limit', 20))
        limit = min(limit, 100)  # Cap at 100
        
        if device_id not in command_history:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'commands': [],
                'count': 0
            }), 200
        
        history = command_history[device_id][-limit:]
        
        logger.info(f"[Command] History requested for {device_id}, returning {len(history)} commands")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'commands': history,
            'count': len(history),
            'total_executed': len(command_history[device_id])
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting command history for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@command_bp.route('/commands/stats', methods=['GET'])
def get_command_statistics():
    """
    Get overall command statistics
    
    Returns aggregate statistics across all devices
    """
    try:
        total_pending = sum(
            len([cmd for cmd in cmds if cmd['status'] == 'pending'])
            for cmds in command_queue.values()
        )
        
        total_executed = sum(len(cmds) for cmds in command_history.values())
        
        success_count = sum(
            len([cmd for cmd in cmds if cmd['status'] == 'success'])
            for cmds in command_history.values()
        )
        
        failed_count = sum(
            len([cmd for cmd in cmds if cmd['status'] in ['failed', 'error']])
            for cmds in command_history.values()
        )
        
        stats = {
            'total_pending': total_pending,
            'total_executed': total_executed,
            'success_count': success_count,
            'failed_count': failed_count,
            'success_rate': (success_count / total_executed * 100) if total_executed > 0 else 0,
            'devices_with_pending_commands': len([d for d, cmds in command_queue.items() if any(cmd['status'] == 'pending' for cmd in cmds)]),
            'devices_with_history': len(command_history)
        }
        
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
