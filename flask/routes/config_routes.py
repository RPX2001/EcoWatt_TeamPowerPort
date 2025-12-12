"""
Configuration Management Routes
Handles remote configuration updates following Milestone 4 specification
Based on: Milestone 4 - Remote Configuration Message Format
"""

from flask import Blueprint, jsonify, request
import logging
import time
from typing import Dict, List
from database import convert_utc_to_local, Database
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
config_bp = Blueprint('config', __name__)

# In-memory configuration storage (in production, use database)
device_configs: Dict[str, dict] = {}
config_history: Dict[str, list] = {}


def load_configs_from_database():
    """
    Load all device configurations from database into memory cache.
    Loads the latest acknowledged configuration for each device.
    Falls back to default config if no acknowledged config exists.
    """
    global device_configs
    device_configs.clear()
    
    # Get all devices
    devices = Database.get_all_devices()
    
    for device in devices:
        device_id = device['device_id']
        
        # Try to load the latest acknowledged (active) configuration
        active_config = Database.get_latest_active_config(device_id)
        
        if active_config:
            device_configs[device_id] = active_config
            logger.info(f"[Config] Loaded active config for {device_id} from database")
        else:
            # No active config found, use default
            device_configs[device_id] = get_default_config()
            logger.info(f"[Config] No active config for {device_id}, using default")
    
    logger.info(f"Loaded configurations for {len(device_configs)} device(s) from database")
    return len(device_configs)


# Available registers from Inverter SIM API Documentation
# Based on Section 4: Modbus Data Registers
# Using actual register names as stored in database (from REGISTER_NAMES in data_utils.py)
AVAILABLE_REGISTERS = [
    {'id': 'Vac1', 'name': 'Vac1 /L1 Phase voltage', 'address': 0, 'gain': 10, 'unit': 'V'},
    {'id': 'Iac1', 'name': 'Iac1 /L1 Phase current', 'address': 1, 'gain': 10, 'unit': 'A'},
    {'id': 'Fac1', 'name': 'Fac1 /L1 Phase frequency', 'address': 2, 'gain': 100, 'unit': 'Hz'},
    {'id': 'Vpv1', 'name': 'Vpv1 /PV1 input voltage', 'address': 3, 'gain': 10, 'unit': 'V'},
    {'id': 'Vpv2', 'name': 'Vpv2 /PV2 input voltage', 'address': 4, 'gain': 10, 'unit': 'V'},
    {'id': 'Ipv1', 'name': 'Ipv1 /PV1 input current', 'address': 5, 'gain': 10, 'unit': 'A'},
    {'id': 'Ipv2', 'name': 'Ipv2 /PV2 input current', 'address': 6, 'gain': 10, 'unit': 'A'},
    {'id': 'Temp', 'name': 'Inverter internal temperature', 'address': 7, 'gain': 10, 'unit': '°C'},
    {'id': 'Pow', 'name': 'Set the export power percentage', 'address': 8, 'gain': 1, 'unit': '%', 'writable': True},
    {'id': 'Pac', 'name': 'Pac L /Inverter current output power', 'address': 9, 'gain': 1, 'unit': 'W'},
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
        'energy_poll_interval': 300,      # Energy self-report interval in seconds (5 minutes = 300s)
        'compression_enabled': True,
        'registers': ['Vac1', 'Iac1', 'Pac'],  # Default registers using actual names
        'power_management': {
            'enabled': False,             # Power management disabled by default
            'techniques': 0x08            # Peripheral Gating (0x08)
        }
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

    # Validate sampling_interval (min 1s, max 300s, must be integer)
    if 'sampling_interval' in config:
        val = config['sampling_interval']
        if isinstance(val, int) and 1 <= val <= 300:
            accepted.append('sampling_interval')
        else:
            rejected.append('sampling_interval')
            errors.append('sampling_interval must be an integer between 1 and 300 seconds')

    # Validate upload_interval (min 1s, max 3600s, must be integer)
    if 'upload_interval' in config:
        val = config['upload_interval']
        if isinstance(val, int) and 1 <= val <= 3600:
            accepted.append('upload_interval')
        else:
            rejected.append('upload_interval')
            errors.append('upload_interval must be an integer between 1 and 3600 seconds')

    # Validate firmware_check_interval (min 1s, max 86400s, must be integer)
    if 'firmware_check_interval' in config:
        val = config['firmware_check_interval']
        if isinstance(val, int) and 1 <= val <= 86400:
            accepted.append('firmware_check_interval')
        else:
            rejected.append('firmware_check_interval')
            errors.append('firmware_check_interval must be an integer between 1 and 86400 seconds')

    # Validate command_poll_interval (min 1s, max 300s, must be integer)
    if 'command_poll_interval' in config:
        val = config['command_poll_interval']
        if isinstance(val, int) and 1 <= val <= 300:
            accepted.append('command_poll_interval')
        else:
            rejected.append('command_poll_interval')
            errors.append('command_poll_interval must be an integer between 1 and 300 seconds')

    # Validate config_poll_interval (min 1s, max 300s, must be integer)
    if 'config_poll_interval' in config:
        val = config['config_poll_interval']
        if isinstance(val, int) and 1 <= val <= 300:
            accepted.append('config_poll_interval')
        else:
            rejected.append('config_poll_interval')
            errors.append('config_poll_interval must be an integer between 1 and 300 seconds')

    # Validate energy_poll_interval (min 1s, max 3600s, must be integer)
    if 'energy_poll_interval' in config:
        val = config['energy_poll_interval']
        if isinstance(val, int) and 1 <= val <= 3600:
            accepted.append('energy_poll_interval')
        else:
            rejected.append('energy_poll_interval')
            errors.append('energy_poll_interval must be an integer between 1 and 3600 seconds')

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

    # Validate power_management
    if 'power_management' in config:
        pm = config['power_management']
        logger.info(f"[Config] Validating power_management: {pm}")
        
        if isinstance(pm, dict):
            pm_valid = True
            # Validate enabled (must be present and be boolean)
            if 'enabled' not in pm:
                pm_valid = False
                errors.append('power_management.enabled is required')
                logger.warning(f"[Config] power_management.enabled is missing")
            elif not isinstance(pm['enabled'], bool):
                pm_valid = False
                errors.append('power_management.enabled must be a boolean')
                logger.warning(f"[Config] power_management.enabled is not a boolean: {type(pm['enabled'])}")
            
            # Validate techniques (must be present and be 0x00 to 0x0F)
            if 'techniques' not in pm:
                pm_valid = False
                errors.append('power_management.techniques is required')
                logger.warning(f"[Config] power_management.techniques is missing")
            else:
                techniques = pm['techniques']
                if isinstance(techniques, str) and techniques.startswith('0x'):
                    techniques = int(techniques, 16)
                if not isinstance(techniques, int) or techniques < 0x00 or techniques > 0x0F:
                    pm_valid = False
                    errors.append(f'power_management.techniques must be 0x00-0x0F (got: {techniques})')
                    logger.warning(f"[Config] power_management.techniques out of range: {techniques}")
            
            if pm_valid:
                accepted.append('power_management')
                logger.info(f"[Config] power_management validation PASSED")
            else:
                rejected.append('power_management')
                logger.warning(f"[Config] power_management validation FAILED: {errors}")
        else:
            rejected.append('power_management')
            errors.append('power_management must be a dict with {enabled, techniques}')
            logger.warning(f"[Config] power_management is not a dict: {type(pm)}")

    return len(rejected) == 0, accepted, rejected, unchanged, errors


@config_bp.route('/config/<device_id>', methods=['GET'])
def get_config(device_id):
    """
    Get configuration for a device
    
    Returns BOTH current running config AND pending config (if any):
    - Dashboard uses 'config' field to display current running config
    - ESP32 uses 'pending_config' field when 'is_pending' is true
    
    This way both dashboard and ESP32 get what they need in one response.
    """
    try:
        from database import Database
        
        # Get current running config (what ESP32 is currently using)
        config_is_default = device_id not in device_configs
        
        if config_is_default:
            current_config = get_default_config()
        else:
            current_config = device_configs[device_id]
        
        logger.info(f"[Config] GET request for {device_id}, returning config: {current_config}")
        
        # Check if there's a pending config in database (what ESP32 should apply next)
        pending_config_data = Database.get_pending_config(device_id)
        
        response = {
            'success': True,
            'device_id': device_id,
            'config': current_config,  # Current running config (for dashboard display)
            'is_default': config_is_default,
            'is_pending': pending_config_data is not None,
            'available_registers': AVAILABLE_REGISTERS,
            'timestamp': time.time()
        }
        
        # If there's a pending config, include it for ESP32 to apply
        if pending_config_data:
            pending = pending_config_data['config']
            
            response['pending_config'] = pending
            response['pending_created_at'] = pending_config_data['created_at']
            logger.info(f"[Config] Returning CURRENT config for display + PENDING config for ESP32 (device: {device_id}, pending created: {pending_config_data['created_at']})")
        else:
            logger.info(f"[Config] Configuration requested for device: {device_id} (is_default: {config_is_default}, no pending updates)")
        
        return jsonify(response), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting config for {device_id}: {e}")
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
    logger.info(f"[Config] ========== UPDATE CONFIG REQUEST RECEIVED ==========")
    logger.info(f"[Config] Device ID: {device_id}")
    logger.info(f"[Config] Method: {request.method}")
    logger.info(f"[Config] Content-Type: {request.content_type}")
    
    try:
        from database import Database
        
        data = request.get_json()
        logger.info(f"[Config] Raw request data: {data}")
        
        if not data or 'config_update' not in data:
            return jsonify({
                'success': False,
                'error': 'Missing config_update in request body'
            }), 400
        
        config_update = data['config_update']
        
        logger.info(f"[Config] Received config update for {device_id}")
        logger.info(f"[Config] Config update contents: {config_update}")
        
        # Get current config or create default
        if device_id not in device_configs:
            device_configs[device_id] = get_default_config()
        
        current_config = device_configs[device_id].copy()
        
        logger.info(f"[Config] Current config before update: {current_config}")
        
        # Validate new configuration
        is_valid, accepted, rejected, unchanged, errors = validate_config(config_update)
        
        logger.info(f"[Config] Validation results - Accepted: {accepted}, Rejected: {rejected}, Errors: {errors}")
        
        # Apply accepted parameters
        actually_changed = []
        for param in accepted[:]:  # Use a copy to avoid modification during iteration
            old_value = current_config.get(param)
            new_value = config_update[param]
            
            if old_value == new_value:
                unchanged.append(param)
            else:
                current_config[param] = new_value
                actually_changed.append(param)
                logger.info(f"[Config] Applied change: {param} = {new_value} (was: {old_value})")
        
        # Update accepted list to only include actually changed params
        accepted = actually_changed
        
        # Store updated config in memory
        device_configs[device_id] = current_config
        
        logger.info(f"[Config] Updated device_configs[{device_id}]: {device_configs[device_id]}")
        
        # Prepare config to save to database (Milestone 4 format with config_update wrapper)
        config_to_save = {'config_update': config_update}
        
        # Save to database with config_update wrapper (Milestone 4 format)
        Database.save_config(device_id=device_id, config=config_to_save)
        
        # Log configuration change in memory history
        if device_id not in config_history:
            config_history[device_id] = []
        
        history_entry = {
            'timestamp': time.time(),
            'config_update': config_update,  # This contains ALL fields including power_management
            'accepted': accepted,
            'rejected': rejected,
            'unchanged': unchanged,
            'errors': errors
        }
        
        config_history[device_id].append(history_entry)
        
        logger.info(f"[Config] Added to config_history: timestamp={history_entry['timestamp']}, accepted={accepted}")
        logger.info(f"[Config] Config update in history: {history_entry['config_update']}")
        
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
        logger.error(f"✗ Error updating config for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@config_bp.route('/config/<device_id>/history', methods=['GET'])
def get_config_history(device_id):
    """
    Get configuration change history for a device from database
    
    Query parameters:
        limit: Number of history entries to return (default: 10, max: 100)
    """
    try:
        from database import Database
        
        limit = int(request.args.get('limit', 10))
        limit = min(limit, 100)  # Cap at 100
        
        # Get config history from database (persistent across server restarts)
        history = Database.get_config_history(device_id, limit)
        
        # Convert timestamps to Sri Lanka time
        for entry in history:
            entry['created_at'] = convert_utc_to_local(entry['created_at'])
            entry['acknowledged_at'] = convert_utc_to_local(entry['acknowledged_at'])
        
        logger.info(f"[Config] History requested for {device_id}, returning {len(history)} entries from database")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'history': history,
            'count': len(history)
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting config history for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@config_bp.route('/config/<device_id>/current', methods=['POST'])
def receive_current_config(device_id):
    """
    Receive current running configuration from ESP32
    This allows us to know what ESP32 is actually running
    
    Expected JSON body:
    {
        "sampling_interval": 2,
        "upload_interval": 15,
        "registers": ["Vac1", "Iac1", "Pac"],
        "compression_enabled": true,
        "timestamp": 1234567890
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
        
        logger.info(f"[Config] Received current config from {device_id}: {data}")
        
        # Get existing config or create default
        if device_id not in device_configs:
            device_configs[device_id] = get_default_config()
        
        # Update (merge) the current config with reported values
        device_configs[device_id].update({
            'sampling_interval': data.get('sampling_interval', device_configs[device_id].get('sampling_interval', 2)),
            'upload_interval': data.get('upload_interval', device_configs[device_id].get('upload_interval', 15)),
            'firmware_check_interval': data.get('firmware_check_interval', device_configs[device_id].get('firmware_check_interval', 60)),
            'command_poll_interval': data.get('command_poll_interval', device_configs[device_id].get('command_poll_interval', 10)),
            'config_poll_interval': data.get('config_poll_interval', device_configs[device_id].get('config_poll_interval', 5)),
            'energy_poll_interval': data.get('energy_poll_interval', device_configs[device_id].get('energy_poll_interval', 300)),
            'compression_enabled': data.get('compression_enabled', device_configs[device_id].get('compression_enabled', True)),
            'registers': data.get('registers', device_configs[device_id].get('registers', []))
        })
        
        # If power_management is included in ESP32's report, update it
        if 'power_management' in data:
            device_configs[device_id]['power_management'] = data['power_management']
        
        logger.info(f"[Config] Updated device_configs[{device_id}]: {device_configs[device_id]}")
        
        # If there was a pending config and ESP32 is reporting this config,
        # it means ESP32 has applied it - mark as acknowledged
        pending = Database.get_pending_config(device_id)
        if pending:
            # Check if the reported config matches the pending config
            pending_regs = set(pending['config'].get('registers', []))
            current_regs = set(data.get('registers', []))
            
            if pending_regs == current_regs:
                # Registers match - mark as applied
                Database.update_config_status(
                    device_id=device_id,
                    status='applied',
                    error_msg=None
                )
                logger.info(f"[Config] ESP32 {device_id} confirmed config application")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'message': 'Current config received',
            'timestamp': time.time()
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error receiving current config for {device_id}: {e}")
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
        power_management = data.get('power_management')  # Optional power management config
        
        # Update database with acknowledgment
        success = Database.update_config_status(
            device_id=device_id,
            status=status,
            error_msg=error_msg
        )
        
        if success:
            # Also update in-memory history with acknowledgment timestamp
            if device_id in config_history and len(config_history[device_id]) > 0:
                # Update the most recent entry (the one that was acknowledged)
                config_history[device_id][-1]['status'] = status
                config_history[device_id][-1]['acknowledged_at'] = int(time.time())
                if error_msg:
                    config_history[device_id][-1]['error_msg'] = error_msg
            
            # Update power management configuration in power_routes if provided
            if power_management:
                # Import power_configs from power_routes
                from routes.power_routes import power_configs
                if device_id not in power_configs:
                    power_configs[device_id] = {}
                power_configs[device_id]['enabled'] = power_management.get('enabled', False)
                power_configs[device_id]['techniques'] = power_management.get('techniques', 0)
                power_configs[device_id]['energy_poll_freq'] = power_management.get('energy_poll_freq', 300000)
                logger.info(f"[Config] Updated power management for {device_id}: enabled={power_management.get('enabled')}, techniques=0x{power_management.get('techniques', 0):02X}")
            
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
        logger.error(f"✗ Error acknowledging config for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
