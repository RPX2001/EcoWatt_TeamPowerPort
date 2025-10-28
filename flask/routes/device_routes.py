"""
Device Management Routes
Handles device registration, listing, and management
"""

from flask import Blueprint, jsonify, request
import logging
import time
from typing import Dict, List

logger = logging.getLogger(__name__)

# Create blueprint
device_bp = Blueprint('device', __name__)

# In-memory device registry (in production, use database)
devices_registry: Dict[str, dict] = {}


def initialize_mock_devices():
    """Initialize a single mock device for testing"""
    import time
    
    mock_devices = [
        {
            'device_id': 'ESP32_001',
            'device_name': 'EcoWatt Device',
            'location': 'Building A - Floor 1',
            'description': 'Primary energy monitor and inverter interface',
            'status': 'active',
            'firmware_version': '1.0.4',
            'registered_at': time.time() - 86400,  # 1 day ago
            'last_seen': time.time() - 30  # 30 seconds ago
        }
    ]
    
    for device in mock_devices:
        device_id = device.pop('device_id')
        devices_registry[device_id] = device
    
    logger.info(f"Initialized {len(mock_devices)} mock device(s)")


# Initialize mock devices on module load
initialize_mock_devices()


@device_bp.route('/devices', methods=['GET'])
def get_devices():
    """
    Get list of all registered devices
    
    Returns:
        JSON with list of devices
    """
    try:
        devices_list = [
            {
                'device_id': device_id,
                **device_info
            }
            for device_id, device_info in devices_registry.items()
        ]
        
        return jsonify({
            'success': True,
            'devices': devices_list,
            'count': len(devices_list)
        }), 200
        
    except Exception as e:
        logger.error(f"Error fetching devices: {e}")
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
        logger.error(f"Error fetching device {device_id}: {e}")
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
        logger.error(f"Error registering device: {e}")
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
        logger.error(f"Error updating device {device_id}: {e}")
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
        logger.error(f"Error deleting device {device_id}: {e}")
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
