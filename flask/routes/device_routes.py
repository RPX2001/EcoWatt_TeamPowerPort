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


def ensure_device_registered(device_id: str, firmware_version: str = None) -> bool:
    """
    Ensure device is registered in database and memory cache.
    This function auto-registers devices when they communicate with the server.
    
    Args:
        device_id: Device identifier
        firmware_version: Optional firmware version to update
        
    Returns:
        True if device was newly registered, False if already existed
    """
    try:
        logger.info(f"[Auto-Register] Checking device: {device_id}")
        
        # Check if device exists in database
        db_device = Database.get_device(device_id)
        
        if not db_device:
            # New device - auto-register it
            logger.info(f"[Auto-Register] Device {device_id} not found in database - registering now")
            Database.save_device(
                device_id=device_id,
                device_name=f'EcoWatt {device_id}',
                location='Auto-registered',
                description=f'Automatically registered on first communication',
                status='active',
                firmware_version=firmware_version or 'unknown'
            )
            logger.info(f"✓ Auto-registered new device: {device_id}")
            
            # Reload devices into memory cache
            load_devices_from_database()
            return True
        else:
            # Device exists - update last_seen and firmware version if provided
            logger.info(f"[Auto-Register] Device {device_id} exists - updating last_seen")
            Database.update_device_last_seen(device_id)
            
            if firmware_version and firmware_version != db_device.get('firmware_version'):
                Database.update_device_firmware(device_id, firmware_version)
                logger.info(f"Updated firmware version for {device_id}: {firmware_version}")
            
            # Update memory cache last_seen
            if device_id in devices_registry:
                devices_registry[device_id]['last_seen'] = time.time()
                if firmware_version:
                    devices_registry[device_id]['firmware_version'] = firmware_version
            
            return False
    except Exception as e:
        logger.error(f"[Auto-Register] Error ensuring device registered: {e}", exc_info=True)
        return False


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


@device_bp.route('/devices/<device_id>/logs', methods=['GET'])
def get_device_logs(device_id: str):
    """
    Get comprehensive device activity logs from all tables
    Returns timestamped log entries showing data uploads, commands, config, OTA, faults, etc.
    """
    try:
        limit = request.args.get('limit', default=100, type=int)
        limit = min(limit, 1000)  # Cap at 1000
        
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        all_logs = []
        
        # Get a balanced mix from each table
        # Divide limit among 5 active tables (no fault_injections data)
        per_table_limit = max(10, limit // 5)
        
        # 1. Sensor Data Uploads
        cursor.execute('''
            SELECT 
                timestamp,
                register_data,
                compression_method,
                compression_ratio,
                created_at
            FROM sensor_data
            WHERE device_id = ?
            ORDER BY timestamp DESC
            LIMIT ?
        ''', (device_id, per_table_limit))
        
        for row in cursor.fetchall():
            import json
            register_data = json.loads(row['register_data'])
            
            all_logs.append({
                'timestamp': row['timestamp'],
                'level': 'INFO',
                'type': 'DATA_UPLOAD',
                'message': f"Data uploaded - {len(register_data)} registers",
                'details': {
                    'registers': list(register_data.keys()),
                    'compression_method': row['compression_method'],
                    'compression_ratio': row['compression_ratio'],
                    'values': register_data
                }
            })
        
        # 2. Commands
        cursor.execute('''
            SELECT 
                created_at,
                command,
                status,
                result,
                error_msg
            FROM commands
            WHERE device_id = ?
            ORDER BY created_at DESC
            LIMIT ?
        ''', (device_id, per_table_limit))
        
        for row in cursor.fetchall():
            level = 'SUCCESS' if row['status'] == 'completed' else 'ERROR' if row['status'] == 'failed' else 'WARNING'
            all_logs.append({
                'timestamp': row['created_at'],
                'level': level,
                'type': 'COMMAND',
                'message': f"Command: {row['command']} - {row['status']}",
                'details': {
                    'command': row['command'],
                    'status': row['status'],
                    'result': row['result'],
                    'error': row['error_msg']
                }
            })
        
        # 3. Configuration Changes
        cursor.execute('''
            SELECT 
                created_at,
                config,
                status
            FROM configurations
            WHERE device_id = ?
            ORDER BY created_at DESC
            LIMIT ?
        ''', (device_id, per_table_limit))
        
        for row in cursor.fetchall():
            import json
            config = json.loads(row['config']) if row['config'] else {}
            all_logs.append({
                'timestamp': row['created_at'],
                'level': 'INFO',
                'type': 'CONFIG_CHANGE',
                'message': f"Configuration {row['status']}",
                'details': {
                    'status': row['status'],
                    'config_keys': list(config.keys())
                }
            })
        
        # 4. OTA Updates
        cursor.execute('''
            SELECT 
                initiated_at,
                download_completed_at,
                from_version,
                to_version,
                status,
                chunks_downloaded,
                chunks_total,
                error_msg
            FROM ota_updates
            WHERE device_id = ?
            ORDER BY initiated_at DESC
            LIMIT ?
        ''', (device_id, per_table_limit))
        
        for row in cursor.fetchall():
            level = 'SUCCESS' if row['status'] == 'completed' else 'ERROR' if row['status'] == 'failed' else 'WARNING'
            # Safe progress calculation - handle None values
            if row['chunks_total'] and row['chunks_total'] > 0 and row['chunks_downloaded'] is not None:
                progress = (row['chunks_downloaded'] / row['chunks_total'] * 100)
            else:
                progress = 0
            
            all_logs.append({
                'timestamp': row['initiated_at'],
                'level': level,
                'type': 'OTA_UPDATE',
                'message': f"OTA Update: {row['from_version']} → {row['to_version']} - {row['status']}",
                'details': {
                    'from_version': row['from_version'],
                    'to_version': row['to_version'],
                    'status': row['status'],
                    'progress': round(progress, 1),
                    'error': row['error_msg']
                }
            })
        
        # 5. Fault Injections (Testing)
        cursor.execute('''
            SELECT 
                created_at,
                fault_type,
                backend,
                success
            FROM fault_injections
            WHERE device_id = ?
            ORDER BY created_at DESC
            LIMIT ?
        ''', (device_id, per_table_limit))
        
        for row in cursor.fetchall():
            all_logs.append({
                'timestamp': row['created_at'],
                'level': 'WARNING',
                'type': 'FAULT_INJECTION',
                'message': f"Fault Test: {row['fault_type']} - {'Success' if row['success'] else 'Failed'}",
                'details': {
                    'fault_type': row['fault_type'],
                    'backend': row['backend'],
                    'success': bool(row['success'])
                }
            })
        
        # 6. Fault Recovery Events
        cursor.execute('''
            SELECT 
                timestamp,
                fault_type,
                recovery_action,
                success
            FROM fault_recovery_events
            WHERE device_id = ?
            ORDER BY timestamp DESC
            LIMIT ?
        ''', (device_id, per_table_limit))
        
        for row in cursor.fetchall():
            level = 'SUCCESS' if row['success'] else 'ERROR'
            all_logs.append({
                'timestamp': row['timestamp'],
                'level': level,
                'type': 'FAULT_RECOVERY',
                'message': f"Recovery: {row['fault_type']} - {row['recovery_action']} - {'Success' if row['success'] else 'Failed'}",
                'details': {
                    'fault_type': row['fault_type'],
                    'recovery_action': row['recovery_action'],
                    'success': bool(row['success'])
                }
            })
        
        # Sort all logs by timestamp (newest first)
        # Convert all timestamps to strings for consistent sorting
        all_logs.sort(key=lambda x: str(x['timestamp']) if x['timestamp'] else '', reverse=True)
        
        # Limit to requested number
        all_logs = all_logs[:limit]
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'logs': all_logs,
            'count': len(all_logs)
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting logs for device {device_id}: {e}")
        logger.exception(e)
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
