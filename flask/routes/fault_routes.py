"""
Fault Injection Routes
Handles fault injection testing with dual backend support:
1. Inverter SIM API (http://20.15.114.131:8080/api/inverter/error) - For Modbus/Inverter faults
2. Flask Local Endpoint - For other fault types (network, MQTT, etc.)

Based on: In21-EN4440-API Service Documentation Section 8: Error Emulation API
"""

from flask import Blueprint, jsonify, request
import logging
import requests
import time
from typing import Dict

logger = logging.getLogger(__name__)

# Create blueprint
fault_bp = Blueprint('fault', __name__)

# Inverter SIM API configuration
INVERTER_SIM_ERROR_API = "http://20.15.114.131:8080/api/inverter/error"

# Local fault tracking (for non-Inverter faults)
active_faults: Dict[str, dict] = {}
fault_statistics: Dict[str, int] = {
    'total_injected': 0,
    'inverter_sim_faults': 0,
    'local_faults': 0,
    'active_faults': 0
}


@fault_bp.route('/fault/inject', methods=['POST'])
def inject_fault():
    """
    Inject a fault for testing
    
    Routes to appropriate backend:
    - Inverter SIM API: EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY (Modbus-related)
    - Local Flask: network, mqtt, command, ota (application-level)
    
    Request Body:
    {
      "slaveAddress": 1,          # For Inverter SIM API
      "functionCode": 3,          # For Inverter SIM API
      "errorType": "EXCEPTION",   # EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY
      "exceptionCode": 1,         # Required if errorType=EXCEPTION
      "delayMs": 0                # Required if errorType=DELAY
    }
    
    OR for local faults:
    {
      "fault_type": "network",    # network, mqtt, command, ota
      "target": "upload",         # Specific target
      "duration": 10              # Duration in seconds
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                'success': False,
                'error': 'Request body is required'
            }), 400
        
        # Check if this is an Inverter SIM fault
        inverter_sim_types = ['EXCEPTION', 'CRC_ERROR', 'CORRUPT', 'PACKET_DROP', 'DELAY']
        error_type = data.get('errorType', data.get('fault_type', '')).upper()
        
        if error_type in inverter_sim_types:
            # Route to Inverter SIM API
            return inject_inverter_sim_fault(data)
        else:
            # Handle locally
            return inject_local_fault(data)
        
    except Exception as e:
        logger.error(f"Error injecting fault: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


def inject_inverter_sim_fault(data):
    """
    Inject fault using Inverter SIM API
    
    API Endpoint: http://20.15.114.131:8080/api/inverter/error
    
    Error Types:
    - EXCEPTION: Modbus exception with exception code
    - CRC_ERROR: CRC checksum error
    - CORRUPT: Corrupted response
    - PACKET_DROP: Dropped packet (no response)
    - DELAY: Response delay in milliseconds
    """
    try:
        # Validate required fields
        slave_address = data.get('slaveAddress', data.get('slaveId', 1))
        function_code = data.get('functionCode', 3)
        error_type = data.get('errorType', '').upper()
        
        if not error_type:
            return jsonify({
                'success': False,
                'error': 'errorType is required'
            }), 400
        
        # Build request for Inverter SIM API
        inverter_request = {
            'slaveAddress': slave_address,
            'functionCode': function_code,
            'errorType': error_type,
            'exceptionCode': data.get('exceptionCode', 0),
            'delayMs': data.get('delayMs', 0)
        }
        
        # Validate specific requirements
        if error_type == 'EXCEPTION' and 'exceptionCode' not in data:
            return jsonify({
                'success': False,
                'error': 'exceptionCode is required for EXCEPTION error type'
            }), 400
        
        if error_type == 'DELAY' and 'delayMs' not in data:
            return jsonify({
                'success': False,
                'error': 'delayMs is required for DELAY error type'
            }), 400
        
        # Call Inverter SIM Error API
        logger.info(f"[Fault] Calling Inverter SIM API: {INVERTER_SIM_ERROR_API}")
        logger.info(f"[Fault] Request: {inverter_request}")
        
        response = requests.post(
            INVERTER_SIM_ERROR_API,
            json=inverter_request,
            timeout=10
        )
        
        if response.status_code == 200:
            result = response.json()
            
            # Update statistics
            fault_statistics['total_injected'] += 1
            fault_statistics['inverter_sim_faults'] += 1
            
            logger.info(f"[Fault] Inverter SIM fault injected: {error_type}")
            
            return jsonify({
                'success': True,
                'message': f'Fault injected via Inverter SIM API: {error_type}',
                'error_type': error_type,
                'backend': 'inverter_sim',
                'inverter_response': result,
                'note': 'Fault will affect next Modbus communication'
            }), 200
        else:
            logger.error(f"[Fault] Inverter SIM API error: {response.status_code}")
            return jsonify({
                'success': False,
                'error': f'Inverter SIM API returned status {response.status_code}',
                'details': response.text
            }), response.status_code
        
    except requests.exceptions.Timeout:
        logger.error("[Fault] Inverter SIM API timeout")
        return jsonify({
            'success': False,
            'error': 'Inverter SIM API timeout'
        }), 504
    except requests.exceptions.ConnectionError:
        logger.error("[Fault] Cannot connect to Inverter SIM API")
        return jsonify({
            'success': False,
            'error': 'Cannot connect to Inverter SIM API',
            'note': 'Check if Inverter SIM is running at http://20.15.114.131:8080'
        }), 503
    except Exception as e:
        logger.error(f"[Fault] Error calling Inverter SIM API: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


def inject_local_fault(data):
    """
    Inject fault locally (non-Inverter faults)
    
    Fault Types:
    - network: Network connectivity issues
    - mqtt: MQTT communication faults
    - command: Command execution faults
    - ota: OTA update faults
    """
    try:
        fault_type = data.get('fault_type', '').lower()
        target = data.get('target', 'all')
        duration = data.get('duration', 60)  # Default 60 seconds
        
        if not fault_type:
            return jsonify({
                'success': False,
                'error': 'fault_type is required'
            }), 400
        
        # Create fault entry
        fault_id = f"{fault_type}_{target}_{int(time.time())}"
        
        fault_entry = {
            'fault_id': fault_id,
            'fault_type': fault_type,
            'target': target,
            'duration': duration,
            'enabled_at': time.time(),
            'expires_at': time.time() + duration,
            'parameters': {k: v for k, v in data.items() if k not in ['fault_type', 'target', 'duration']}
        }
        
        active_faults[fault_id] = fault_entry
        
        # Update statistics
        fault_statistics['total_injected'] += 1
        fault_statistics['local_faults'] += 1
        fault_statistics['active_faults'] = len(active_faults)
        
        logger.info(f"[Fault] Local fault injected: {fault_type} -> {target} for {duration}s")
        
        return jsonify({
            'success': True,
            'message': f'Local fault injected: {fault_type}',
            'fault_id': fault_id,
            'fault_type': fault_type,
            'target': target,
            'duration': duration,
            'backend': 'local_flask',
            'expires_at': fault_entry['expires_at']
        }), 201
        
    except Exception as e:
        logger.error(f"[Fault] Error injecting local fault: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/status', methods=['GET'])
def get_fault_status():
    """
    Get status of active faults
    
    Returns information about currently active faults (local only)
    """
    try:
        # Clean up expired faults
        current_time = time.time()
        expired_faults = [
            fault_id for fault_id, fault in active_faults.items()
            if fault['expires_at'] < current_time
        ]
        
        for fault_id in expired_faults:
            del active_faults[fault_id]
        
        fault_statistics['active_faults'] = len(active_faults)
        
        return jsonify({
            'success': True,
            'active_faults': list(active_faults.values()),
            'count': len(active_faults),
            'statistics': fault_statistics
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting fault status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/clear', methods=['POST', 'DELETE'])
def clear_faults():
    """
    Clear all active faults or specific fault by ID
    
    Query parameters:
        fault_id: Specific fault ID to clear (optional)
    """
    try:
        fault_id = request.args.get('fault_id')
        
        if fault_id:
            if fault_id in active_faults:
                del active_faults[fault_id]
                logger.info(f"[Fault] Cleared fault: {fault_id}")
                message = f'Fault cleared: {fault_id}'
            else:
                return jsonify({
                    'success': False,
                    'error': f'Fault not found: {fault_id}'
                }), 404
        else:
            count = len(active_faults)
            active_faults.clear()
            logger.info(f"[Fault] Cleared all {count} faults")
            message = f'Cleared {count} active faults'
        
        fault_statistics['active_faults'] = len(active_faults)
        
        return jsonify({
            'success': True,
            'message': message
        }), 200
        
    except Exception as e:
        logger.error(f"Error clearing faults: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/types', methods=['GET'])
def get_available_fault_types():
    """
    Get list of available fault types and their descriptions
    """
    try:
        fault_types = {
            'inverter_sim': {
                'backend': 'Inverter SIM API (http://20.15.114.131:8080)',
                'types': {
                    'EXCEPTION': 'Modbus exception with exception code (01-0B)',
                    'CRC_ERROR': 'CRC checksum error in response',
                    'CORRUPT': 'Corrupted/malformed response data',
                    'PACKET_DROP': 'Dropped packet (no response)',
                    'DELAY': 'Response delay in milliseconds'
                },
                'example': {
                    'slaveAddress': 1,
                    'functionCode': 3,
                    'errorType': 'EXCEPTION',
                    'exceptionCode': 2
                }
            },
            'local_flask': {
                'backend': 'Local Flask Server',
                'types': {
                    'network': 'Network connectivity issues',
                    'mqtt': 'MQTT communication faults',
                    'command': 'Command execution faults',
                    'ota': 'OTA update faults'
                },
                'example': {
                    'fault_type': 'network',
                    'target': 'upload',
                    'duration': 60
                }
            }
        }
        
        return jsonify({
            'success': True,
            'fault_types': fault_types
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting fault types: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


# ============================================================================
# FAULT RECOVERY ENDPOINTS (Milestone 5)
# ============================================================================

# Storage for recovery events (in-memory, indexed by device_id)
recovery_events: Dict[str, list] = {}  # device_id -> list of recovery dicts

# Recovery statistics
recovery_statistics: Dict[str, int] = {
    'total_recoveries': 0,
    'successful_recoveries': 0,
    'failed_recoveries': 0
}


@fault_bp.route('/fault/recovery', methods=['POST'])
def receive_recovery():
    """
    Receive fault recovery event from ESP32
    
    This endpoint is called by ESP32 when it detects and recovers from a fault.
    
    Request Body (JSON):
    {
      "device_id": "ESP32_EcoWatt_Smart",
      "timestamp": 1698527500,
      "fault_type": "crc_error",           # crc_error, truncated, buffer_overflow, garbage
      "recovery_action": "retry_read",     # retry_read, reset_connection, discard_data
      "success": true,
      "details": "CRC validation failed, retried successfully after 1 attempt"
    }
    
    Response:
    {
      "success": true,
      "message": "Recovery event recorded"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                'success': False,
                'error': 'Request body is required'
            }), 400
        
        # Validate required fields
        required_fields = ['device_id', 'timestamp', 'fault_type', 'recovery_action', 'success']
        missing_fields = [field for field in required_fields if field not in data]
        
        if missing_fields:
            return jsonify({
                'success': False,
                'error': f'Missing required fields: {", ".join(missing_fields)}'
            }), 400
        
        device_id = data['device_id']
        
        # Initialize device recovery list if not exists
        if device_id not in recovery_events:
            recovery_events[device_id] = []
        
        # Add recovery event
        recovery_event = {
            'device_id': device_id,
            'timestamp': data['timestamp'],
            'fault_type': data['fault_type'],
            'recovery_action': data['recovery_action'],
            'success': data['success'],
            'details': data.get('details', ''),
            'received_at': int(time.time())  # Server timestamp
        }
        
        recovery_events[device_id].append(recovery_event)
        
        # Update statistics
        recovery_statistics['total_recoveries'] += 1
        if data['success']:
            recovery_statistics['successful_recoveries'] += 1
        else:
            recovery_statistics['failed_recoveries'] += 1
        
        logger.info(f"[Recovery] Event from {device_id}: {data['fault_type']} - {data['recovery_action']} - Success: {data['success']}")
        
        return jsonify({
            'success': True,
            'message': 'Recovery event recorded',
            'event_id': len(recovery_events[device_id])
        }), 200
        
    except Exception as e:
        logger.error(f"Error receiving recovery event: {e}", exc_info=True)
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/recovery/<device_id>', methods=['GET'])
def get_recovery_history(device_id):
    """
    Get recovery event history for a specific device
    
    Query Parameters:
    - limit: Maximum number of events to return (default: 50)
    - offset: Number of events to skip (default: 0)
    - fault_type: Filter by fault type (optional)
    
    Response:
    {
      "device_id": "ESP32_EcoWatt_Smart",
      "total_events": 10,
      "events": [
        {
          "timestamp": 1698527500,
          "fault_type": "crc_error",
          "recovery_action": "retry_read",
          "success": true,
          "details": "...",
          "received_at": 1698527501
        },
        ...
      ],
      "statistics": {
        "total": 10,
        "successful": 8,
        "failed": 2,
        "success_rate": 80.0
      }
    }
    """
    try:
        # Get query parameters
        limit = request.args.get('limit', 50, type=int)
        offset = request.args.get('offset', 0, type=int)
        fault_type_filter = request.args.get('fault_type', None)
        
        # Get events for device
        device_events = recovery_events.get(device_id, [])
        
        # Filter by fault type if specified
        if fault_type_filter:
            device_events = [e for e in device_events if e['fault_type'] == fault_type_filter]
        
        # Calculate statistics
        total = len(device_events)
        successful = sum(1 for e in device_events if e['success'])
        failed = total - successful
        success_rate = (successful / total * 100) if total > 0 else 0.0
        
        # Apply pagination
        paginated_events = device_events[offset:offset + limit]
        
        return jsonify({
            'device_id': device_id,
            'total_events': total,
            'events': paginated_events,
            'statistics': {
                'total': total,
                'successful': successful,
                'failed': failed,
                'success_rate': round(success_rate, 2)
            }
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting recovery history: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/recovery/all', methods=['GET'])
def get_all_recovery_events():
    """
    Get recovery events from all devices
    
    Query Parameters:
    - limit: Maximum number of events per device (default: 10)
    
    Response:
    {
      "devices": {
        "ESP32_EcoWatt_Smart": {
          "total_events": 10,
          "recent_events": [...],
          "statistics": {...}
        },
        ...
      },
      "global_statistics": {
        "total_recoveries": 20,
        "successful_recoveries": 16,
        "failed_recoveries": 4,
        "success_rate": 80.0
      }
    }
    """
    try:
        limit = request.args.get('limit', 10, type=int)
        
        devices_data = {}
        
        for device_id, events in recovery_events.items():
            total = len(events)
            successful = sum(1 for e in events if e['success'])
            failed = total - successful
            success_rate = (successful / total * 100) if total > 0 else 0.0
            
            devices_data[device_id] = {
                'total_events': total,
                'recent_events': events[-limit:],  # Last N events
                'statistics': {
                    'total': total,
                    'successful': successful,
                    'failed': failed,
                    'success_rate': round(success_rate, 2)
                }
            }
        
        # Calculate global success rate
        global_total = recovery_statistics['total_recoveries']
        global_success = recovery_statistics['successful_recoveries']
        global_success_rate = (global_success / global_total * 100) if global_total > 0 else 0.0
        
        return jsonify({
            'devices': devices_data,
            'global_statistics': {
                'total_recoveries': global_total,
                'successful_recoveries': global_success,
                'failed_recoveries': recovery_statistics['failed_recoveries'],
                'success_rate': round(global_success_rate, 2)
            }
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting all recovery events: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/recovery/clear', methods=['POST'])
def clear_recovery_events():
    """
    Clear recovery events (for testing)
    
    Request Body (optional):
    {
      "device_id": "ESP32_EcoWatt_Smart"  # Clear specific device, or omit to clear all
    }
    """
    try:
        data = request.get_json() or {}
        device_id = data.get('device_id')
        
        if device_id:
            # Clear specific device
            if device_id in recovery_events:
                event_count = len(recovery_events[device_id])
                recovery_events[device_id] = []
                logger.info(f"[Recovery] Cleared {event_count} events for {device_id}")
                return jsonify({
                    'success': True,
                    'message': f'Cleared {event_count} events for {device_id}'
                }), 200
            else:
                return jsonify({
                    'success': False,
                    'error': f'No events found for device {device_id}'
                }), 404
        else:
            # Clear all devices
            total_cleared = sum(len(events) for events in recovery_events.values())
            recovery_events.clear()
            recovery_statistics['total_recoveries'] = 0
            recovery_statistics['successful_recoveries'] = 0
            recovery_statistics['failed_recoveries'] = 0
            
            logger.info(f"[Recovery] Cleared all {total_cleared} recovery events")
            return jsonify({
                'success': True,
                'message': f'Cleared all {total_cleared} recovery events'
            }), 200
        
    except Exception as e:
        logger.error(f"Error clearing recovery events: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
