"""
Device Management Routes
Handles device registration, listing, and management
Uses persistent database storage for device registry
"""

from flask import Blueprint, jsonify, request
import logging
import time
from typing import Dict, List
from datetime import datetime
from database import Database
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
device_bp = Blueprint('device', __name__)

# Keep in-memory cache for backward compatibility (auto-syncs with database)
# This is populated from database on startup and kept in sync
devices_registry: Dict[str, dict] = {}


def load_devices_from_database():
    """Load all devices from database into memory cache"""
    global devices_registry
    devices_registry.clear()
    
    devices = Database.get_all_devices()
    for device in devices:
        device_id = device.pop('device_id')
        # Convert datetime to timestamp for JSON compatibility
        if isinstance(device.get('registered_at'), datetime):
            device['registered_at'] = device['registered_at'].timestamp()
        if isinstance(device.get('last_seen'), datetime):
            device['last_seen'] = device['last_seen'].timestamp()
        devices_registry[device_id] = device
    
    logger.info(f"Loaded {len(devices_registry)} device(s) from database")
    return len(devices_registry)


def initialize_mock_devices():
    """Initialize a single mock device for testing (DEPRECATED - using database)"""
    import time
    from database import Database
    
    # Check if any devices exist in database
    existing_devices = Database.get_all_devices()
    if len(existing_devices) > 0:
        logger.info(f"Found {len(existing_devices)} existing device(s) in database")
        load_devices_from_database()
        return
    
    # No devices exist - create a mock device for testing
    mock_device = {
        'device_id': 'ESP32_001',
        'device_name': 'EcoWatt Device',
        'location': 'Building A - Floor 1',
        'description': 'Primary energy monitor and inverter interface',
        'status': 'active',
        'firmware_version': '1.0.4',
        'last_seen': datetime.now()
    }
    
    Database.save_device(**mock_device)
    logger.info("Initialized 1 mock device in database")
    load_devices_from_database()


# Load devices from database on module import
load_devices_from_database()


@device_bp.route('/devices', methods=['GET'])
def get_devices():
    """
    Get list of all registered devices from database
    
    Returns:
        JSON with list of devices
    """
    try:
        # Always fetch fresh data from database
        devices_list = Database.get_all_devices()
        
        # Convert datetime objects to timestamps for JSON
        for device in devices_list:
            if isinstance(device.get('registered_at'), datetime):
                device['registered_at'] = device['registered_at'].timestamp()
            if isinstance(device.get('last_seen'), datetime):
                device['last_seen'] = device['last_seen'].timestamp()
        
        return jsonify({
            'success': True,
            'devices': devices_list,
            'count': len(devices_list)
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error fetching devices: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@device_bp.route('/devices/<device_id>', methods=['GET'])
def get_device(device_id: str):
    """
    Get information about a specific device
    
    Args:
        device_id: Device identifier
        
    Returns:
        JSON with device information
    """
    try:
        if device_id not in devices_registry:
            return jsonify({
                'success': False,
                'error': f'Device {device_id} not found'
            }), 404
        
        device_info = devices_registry[device_id].copy()
        device_info['device_id'] = device_id
        
        return jsonify({
            'success': True,
            'device': device_info
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error fetching device {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@device_bp.route('/devices', methods=['POST'])
def register_device():
    """
    Register a new device
    
    Expected JSON body:
        {
            "device_id": "ESP32_001",
            "device_name": "Living Room Monitor",
            "location": "Building A",
            "description": "Energy monitor for living room"
        }
        
    Returns:
        JSON with registration status
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                'success': False,
                'error': 'No JSON data provided'
            }), 400
        
        device_id = data.get('device_id')
        if not device_id:
            return jsonify({
                'success': False,
                'error': 'device_id is required'
            }), 400
        
        # Check if device already exists
        if device_id in devices_registry:
            # Device exists - update firmware_version if provided
            firmware_version = data.get('firmware_version')
            if firmware_version:
                devices_registry[device_id]['firmware_version'] = firmware_version
                
                # Also update in database
                from database import Database
                db_device = Database.get_device(device_id)
                if db_device:
                    Database.update_device_firmware(device_id, firmware_version)
                
                logger.info(f"Updated firmware version for {device_id}: {firmware_version}")
                
                return jsonify({
                    'success': True,
                    'message': f'Device {device_id} firmware version updated',
                    'device': {
                        'device_id': device_id,
                        **devices_registry[device_id]
                    }
                }), 200
            else:
                return jsonify({
                    'success': False,
                    'error': f'Device {device_id} already registered'
                }), 409
        
        # Register device
        device_info = {
            'device_name': data.get('device_name', f'Device {device_id}'),
            'location': data.get('location', 'Unknown'),
            'description': data.get('description', ''),
            'status': 'active',
            'registered_at': time.time(),
            'last_seen': None,
            'firmware_version': data.get('firmware_version', 'unknown')
        }
        
        devices_registry[device_id] = device_info
        
        logger.info(f"Device registered: {device_id}")
        
        return jsonify({
            'success': True,
            'message': f'Device {device_id} registered successfully',
            'device': {
                'device_id': device_id,
                **device_info
            }
        }), 201
        
    except Exception as e:
        logger.error(f"✗ Error registering device: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@device_bp.route('/devices/<device_id>', methods=['PUT'])
def update_device(device_id: str):
    """
    Update device information
    
    Args:
        device_id: Device identifier
        
    Expected JSON body:
        {
            "device_name": "Updated Name",
            "location": "Updated Location",
            "description": "Updated description",
            "status": "active|inactive|maintenance"
        }
        
    Returns:
        JSON with update status
    """
    try:
        if device_id not in devices_registry:
            return jsonify({
                'success': False,
                'error': f'Device {device_id} not found'
            }), 404
        
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': 'No JSON data provided'
            }), 400
        
        # Update allowed fields
        allowed_fields = ['device_name', 'location', 'description', 'status', 'firmware_version']
        for field in allowed_fields:
            if field in data:
                devices_registry[device_id][field] = data[field]
        
        logger.info(f"Device updated: {device_id}")
        
        return jsonify({
            'success': True,
            'message': f'Device {device_id} updated successfully',
            'device': {
                'device_id': device_id,
                **devices_registry[device_id]
            }
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error updating device {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@device_bp.route('/devices/<device_id>', methods=['DELETE'])
def delete_device(device_id: str):
    """
    Delete (unregister) a device
    
    Args:
        device_id: Device identifier
        
    Returns:
        JSON with deletion status
    """
    try:
        if device_id not in devices_registry:
            return jsonify({
                'success': False,
                'error': f'Device {device_id} not found'
            }), 404
        
        del devices_registry[device_id]
        
        logger.info(f"Device deleted: {device_id}")
        
        return jsonify({
            'success': True,
            'message': f'Device {device_id} deleted successfully'
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error deleting device {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


def update_device_last_seen(device_id: str):
    """
    Update the last_seen timestamp for a device
    Called internally when device communicates
    """
    if device_id in devices_registry:
        devices_registry[device_id]['last_seen'] = time.time()
