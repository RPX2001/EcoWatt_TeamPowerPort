"""
Configuration Management Routes
Handles remote configuration updates following Milestone 4 specification
Based on: Milestone 4 - Remote Configuration Message Format
"""

from flask import Blueprint, jsonify, request
import logging
import time
from typing import Dict, List

logger = logging.getLogger(__name__)

# Create blueprint
config_bp = Blueprint('config', __name__)

# In-memory configuration storage (in production, use database)
device_configs: Dict[str, dict] = {}
config_history: Dict[str, list] = {}

# Available registers from Inverter SIM API Documentation
# Based on Section 4: Modbus Data Registers
AVAILABLE_REGISTERS = [
    {'id': 'voltage', 'name': 'Vac1 /L1 Phase voltage', 'address': 0, 'gain': 10, 'unit': 'V'},
    {'id': 'current', 'name': 'Iac1 /L1 Phase current', 'address': 1, 'gain': 10, 'unit': 'A'},
    {'id': 'frequency', 'name': 'Fac1 /L1 Phase frequency', 'address': 2, 'gain': 100, 'unit': 'Hz'},
    {'id': 'vpv1', 'name': 'Vpv1 /PV1 input voltage', 'address': 3, 'gain': 10, 'unit': 'V'},
    {'id': 'vpv2', 'name': 'Vpv2 /PV2 input voltage', 'address': 4, 'gain': 10, 'unit': 'V'},
    {'id': 'ipv1', 'name': 'Ipv1 /PV1 input current', 'address': 5, 'gain': 10, 'unit': 'A'},
    {'id': 'ipv2', 'name': 'Ipv2 /PV2 input current', 'address': 6, 'gain': 10, 'unit': 'A'},
    {'id': 'temperature', 'name': 'Inverter internal temperature', 'address': 7, 'gain': 10, 'unit': '°C'},
    {'id': 'export_power_pct', 'name': 'Set the export power percentage', 'address': 8, 'gain': 1, 'unit': '%', 'writable': True},
    {'id': 'power', 'name': 'Pac L /Inverter current output power', 'address': 9, 'gain': 1, 'unit': 'W'},
]

# Get list of register IDs for validation
AVAILABLE_REGISTER_IDS = [reg['id'] for reg in AVAILABLE_REGISTERS]


def get_default_config():
    """Get default configuration for new devices"""
    return {
        'sampling_interval': 2,           # Device reads from inverter every 2s
        'upload_interval': 15,            # Device uploads to cloud every 15s
        'firmware_check_interval': 60,    # Check for firmware updates every 60s
        'command_poll_interval': 10,      # Check for pending commands every 10s
        'config_poll_interval': 5,        # Check for config updates every 5s
        'compression_enabled': True,
        'registers': ['voltage', 'current', 'power']  # Default registers to monitor
    }


def validate_config(config: dict) -> tuple:
    """
    Validate configuration parameters
    Returns: (is_valid, accepted_params, rejected_params, unchanged_params, errors)
    """
    accepted = []
    rejected = []
    unchanged = []
    errors = []

    # Validate sampling_interval
    if 'sampling_interval' in config:
        if 1 <= config['sampling_interval'] <= 300:
            accepted.append('sampling_interval')
        else:
            rejected.append('sampling_interval')
            errors.append('sampling_interval must be between 1 and 300 seconds')

    # Validate upload_interval
    if 'upload_interval' in config:
        if 10 <= config['upload_interval'] <= 3600:
            accepted.append('upload_interval')
        else:
            rejected.append('upload_interval')
            errors.append('upload_interval must be between 10 and 3600 seconds')

    # Validate firmware_check_interval
    if 'firmware_check_interval' in config:
        if 30 <= config['firmware_check_interval'] <= 86400:
            accepted.append('firmware_check_interval')
        else:
            rejected.append('firmware_check_interval')
            errors.append('firmware_check_interval must be between 30 and 86400 seconds')

    # Validate command_poll_interval
    if 'command_poll_interval' in config:
        if 5 <= config['command_poll_interval'] <= 300:
            accepted.append('command_poll_interval')
        else:
            rejected.append('command_poll_interval')
            errors.append('command_poll_interval must be between 5 and 300 seconds')

    # Validate config_poll_interval
    if 'config_poll_interval' in config:
        if 1 <= config['config_poll_interval'] <= 300:
            accepted.append('config_poll_interval')
        else:
            rejected.append('config_poll_interval')
            errors.append('config_poll_interval must be between 1 and 300 seconds')

    # Validate compression_enabled
    if 'compression_enabled' in config:
        if isinstance(config['compression_enabled'], bool):
            accepted.append('compression_enabled')
        else:
            rejected.append('compression_enabled')
            errors.append('compression_enabled must be a boolean')

    # Validate registers
    if 'registers' in config:
        if isinstance(config['registers'], list) and len(config['registers']) > 0:
            invalid_registers = [r for r in config['registers'] if r not in AVAILABLE_REGISTER_IDS]
            if not invalid_registers:
                accepted.append('registers')
            else:
                rejected.append('registers')
                errors.append(f'Invalid registers: {invalid_registers}. Available: {AVAILABLE_REGISTER_IDS}')
        else:
            rejected.append('registers')
            errors.append('registers must be a non-empty list')

    return len(rejected) == 0, accepted, rejected, unchanged, errors


@config_bp.route('/config/<device_id>', methods=['GET'])
def get_config(device_id):
    """
    Get current configuration for a device
    
    Returns pending config from database (if any), otherwise current config or default
    ESP32 polls this endpoint to check for configuration updates
    """
    try:
        from database import Database
        
        # First check if there's a pending config in database (takes priority)
        pending_config = Database.get_pending_config(device_id)
        
        if pending_config:
            # Return pending config for ESP32 to apply
            logger.info(f"[Config] Returning PENDING config for {device_id} (created: {pending_config['created_at']})")
            return jsonify({
                'success': True,
                'device_id': device_id,
                'config': pending_config['config'],
                'is_default': False,
                'is_pending': True,
                'available_registers': AVAILABLE_REGISTERS,
                'timestamp': time.time(),
                'note': 'This is a pending configuration update - send acknowledgment after applying'
            }), 200
        
        # No pending config - return current config or default
        config_is_default = device_id not in device_configs
        
        if config_is_default:
            config = get_default_config()
        else:
            config = device_configs[device_id]
        
        logger.info(f"[Config] Configuration requested for device: {device_id} (is_default: {config_is_default}, no pending updates)")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'config': config,
            'is_default': config_is_default,
            'is_pending': False,
            'available_registers': AVAILABLE_REGISTERS,
            'timestamp': time.time()
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting config for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@config_bp.route('/config/<device_id>', methods=['PUT', 'POST'])
def update_config(device_id):
    """
    Update device configuration
    Following Milestone 4 Remote Configuration Message Format:
    
    Cloud → Device:
    {
      "config_update": {
        "sampling_interval": 5,
        "registers": ["voltage", "current", "frequency"]
      }
    }
    
    Device → Cloud Acknowledgment:
    {
      "config_ack": {
        "accepted": ["sampling_interval", "registers"],
        "rejected": [],
        "unchanged": []
      }
    }
    
    Configuration changes take effect after the next upload cycle, without reboot.
    """
    try:
        from database import Database
        
        data = request.get_json()
        
        if not data or 'config_update' not in data:
            return jsonify({
                'success': False,
                'error': 'Missing config_update in request body'
            }), 400
        
        config_update = data['config_update']
        
        # Get current config or create default
        if device_id not in device_configs:
            device_configs[device_id] = get_default_config()
        
        current_config = device_configs[device_id].copy()
        
        # Validate new configuration
        is_valid, accepted, rejected, unchanged, errors = validate_config(config_update)
        
        # Apply accepted parameters
        for param in accepted:
            old_value = current_config.get(param)
            new_value = config_update[param]
            
            if old_value == new_value:
                accepted.remove(param)
                unchanged.append(param)
            else:
                current_config[param] = new_value
        
        # Store updated config in memory
        device_configs[device_id] = current_config
        
        # CRITICAL: Save to database (REPLACES any pending config)
        # This implements the "override not queue" behavior
        Database.save_config(device_id=device_id, config=current_config)
        
        # Log configuration change in memory history
        if device_id not in config_history:
            config_history[device_id] = []
        
        config_history[device_id].append({
            'timestamp': time.time(),
            'config_update': config_update,
            'accepted': accepted,
            'rejected': rejected,
            'unchanged': unchanged,
            'errors': errors
        })
        
        # Keep only last 50 history entries
        if len(config_history[device_id]) > 50:
            config_history[device_id] = config_history[device_id][-50:]
        
        logger.info(f"[Config] Configuration updated for {device_id} (REPLACED pending) - Accepted: {accepted}, Rejected: {rejected}")
        
        # Return acknowledgment following Milestone 4 format
        return jsonify({
            'success': True,
            'device_id': device_id,
            'config_ack': {
                'accepted': accepted,
                'rejected': rejected,
                'unchanged': unchanged,
                'errors': errors if rejected else []
            },
            'current_config': current_config,
            'timestamp': time.time(),
            'note': 'Configuration will take effect after ESP32 polls (latest config replaces previous pending)'
        }), 200
        
    except Exception as e:
        logger.error(f"Error updating config for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@config_bp.route('/config/<device_id>/history', methods=['GET'])
def get_config_history(device_id):
    """
    Get configuration change history for a device
    
    Query parameters:
        limit: Number of history entries to return (default: 10, max: 50)
    """
    try:
        limit = int(request.args.get('limit', 10))
        limit = min(limit, 50)  # Cap at 50
        
        if device_id not in config_history:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'history': [],
                'count': 0
            }), 200
        
        history = config_history[device_id][-limit:]
        
        logger.info(f"[Config] History requested for {device_id}, returning {len(history)} entries")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'history': history,
            'count': len(history),
            'total_changes': len(config_history[device_id])
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting config history for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@config_bp.route('/config/<device_id>/acknowledge', methods=['POST'])
def acknowledge_config(device_id):
    """
    Receive acknowledgment from ESP32 after config is applied
    
    Expected JSON body:
    {
        "status": "applied" | "failed",
        "timestamp": 1234567890,  # Unix timestamp
        "error_msg": "Optional error message if failed"
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
        if 'status' not in data:
            return jsonify({
                'success': False,
                'error': 'Missing required field: status'
            }), 400
        
        status = data['status']
        error_msg = data.get('error_msg')
        
        # Update database with acknowledgment
        success = Database.update_config_status(
            device_id=device_id,
            status=status,
            error_msg=error_msg
        )
        
        if success:
            logger.info(f"[Config] ESP32 acknowledged config for {device_id}: {status}")
            return jsonify({
                'success': True,
                'device_id': device_id,
                'message': 'Config acknowledgment received',
                'status': status
            }), 200
        else:
            logger.warning(f"[Config] No pending config found for {device_id} to acknowledge")
            return jsonify({
                'success': False,
                'error': 'No pending config found to acknowledge'
            }), 404
        
    except Exception as e:
        logger.error(f"Error acknowledging config for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
