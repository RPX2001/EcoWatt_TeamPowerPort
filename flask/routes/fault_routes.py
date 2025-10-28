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
