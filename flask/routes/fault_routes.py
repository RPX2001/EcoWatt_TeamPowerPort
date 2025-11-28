"""
Fault Injection Routes
Handles fault injection testing with dual backend support:
1. Inverter SIM API (http://20.15.114.131:8080/api/user/error-flag/add) - For Modbus/Inverter faults
2. Flask Local Endpoint - For other fault types (network, MQTT, etc.)

Based on: In21-EN4440-API Service Documentation Section 7: Add Error Flag API
"""

from flask import Blueprint, jsonify, request
import logging
import requests
import time
from typing import Dict
from database import Database
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
fault_bp = Blueprint('fault', __name__)

# Inverter SIM API Configuration
# Section 7: Add Error Flag API - Sets flag for NEXT ESP32 request to receive error
INVERTER_SIM_ERROR_FLAG_API = "http://20.15.114.131:8080/api/user/error-flag/add"
# Section 8: Error Emulation API - Returns error frame immediately (testing only)
INVERTER_SIM_ERROR_API = "http://20.15.114.131:8080/api/inverter/error"

# Inverter SIM API Key (from EN4440 API Service Keys spreadsheet)
# This is the API key for the Error Flag API (Section 7)
INVERTER_API_KEY = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ=="

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
        logger.error(f"✗ Error injecting fault: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


def inject_inverter_sim_fault(data):
    """
    Inject fault using Inverter SIM Error Flag API
    
    API Endpoint: http://20.15.114.131:8080/api/user/error-flag/add
    
    This API sets a flag so that the NEXT request from ESP32 to Inverter SIM
    (e.g., /api/inverter/read) will receive a corrupted/error response.
    
    Error Types:
    - EXCEPTION: Modbus exception with exception code
    - CRC_ERROR: CRC checksum error
    - CORRUPT: Corrupted response
    - PACKET_DROP: Dropped packet (no response)
    - DELAY: Response delay in milliseconds
    """
    try:
        # Validate required fields
        error_type = data.get('errorType', '').upper()
        
        if not error_type:
            return jsonify({
                'success': False,
                'error': 'errorType is required'
            }), 400
        
        # Build request for Inverter SIM Error Flag API
        # NOTE: This API does NOT need slaveAddress or functionCode
        # It sets a flag that affects the NEXT ESP32 request
        inverter_request = {
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
        
        # Call Inverter SIM Error Flag API
        logger.info(f"[Fault] Calling Inverter SIM Error Flag API: {INVERTER_SIM_ERROR_FLAG_API}")
        logger.info(f"[Fault] Request: {inverter_request}")
        logger.info(f"[Fault] This will affect the NEXT request from ESP32")
        
        # Add Authorization header (required for Error Flag API)
        headers = {
            'Content-Type': 'application/json',
            'Authorization': INVERTER_API_KEY
        }
        
        response = requests.post(
            INVERTER_SIM_ERROR_FLAG_API,
            json=inverter_request,
            headers=headers,
            timeout=10
        )
        
        if response.status_code == 200:
            # Update statistics
            fault_statistics['total_injected'] += 1
            fault_statistics['inverter_sim_faults'] += 1
            
            # Save to database
            Database.save_fault_injection(
                device_id=None,  # Affects all ESP32s
                fault_type=error_type,
                backend='inverter_sim',
                error_type=error_type,
                exception_code=data.get('exceptionCode'),
                delay_ms=data.get('delayMs'),
                success=True
            )
            
            logger.info(f"[Fault] Inverter SIM error flag set: {error_type}")
            logger.info(f"[Fault] ESP32's next Modbus request will receive corrupted response")
            
            return jsonify({
                'success': True,
                'message': f'Error flag set for Inverter SIM: {error_type}',
                'error_type': error_type,
                'backend': 'inverter_sim',
                'note': 'ESP32 will receive corrupted response on next Modbus request'
            }), 200
        else:
            # Save failed injection to database
            Database.save_fault_injection(
                device_id=None,
                fault_type=error_type,
                backend='inverter_sim',
                error_type=error_type,
                exception_code=data.get('exceptionCode'),
                delay_ms=data.get('delayMs'),
                success=False,
                error_msg=f'API returned {response.status_code}: {response.text}'
            )
            
            logger.error(f"[Fault] Inverter SIM API error: {response.status_code}")
            logger.error(f"[Fault] Response: {response.text}")
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
    
    Fault Types (Milestone 5):
    - network: Network connectivity issues (timeout, disconnect, slow, intermittent)
    - ota: OTA update faults (triggers ESP32 rollback)
    
    OTA Fault Subtypes (only these 3 supported):
        - corrupt_chunk: Corrupt firmware chunk data (CRC/hash validation fails)
        - bad_hash: Incorrect SHA256 hash in manifest (verification fails)  
        - bad_signature: Incorrect signature in manifest (signature check fails)
    
    Network Fault Subtypes:
        - timeout: Connection timeout (delay then fail)
        - disconnect: Connection drop (immediate failure)
        - slow: Slow network speed (add delay)
        - intermittent: Random intermittent failures
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
        
        # Handle OTA faults specially using OTA handler
        if fault_type == 'ota':
            return inject_ota_fault(data)
        
        # Handle network faults using fault_handler
        if fault_type == 'network':
            return inject_network_fault_handler(data)
        
        # Create fault entry for other local faults
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


def inject_network_fault_handler(data):
    """
    Inject network-specific fault using fault_handler (Milestone 5)
    
    Request Body:
    {
      "fault_type": "network",
      "network_fault_subtype": "timeout",  # timeout, disconnect, slow, intermittent
      "target_endpoint": "/power/upload",  # Optional - target specific endpoint
      "probability": 100,                  # Optional - probability (0-100%)
      "parameters": {                      # Optional
        "timeout_ms": 30000,
        "delay_ms": 3000,
        "failure_rate": 0.5
      }
    }
    """
    try:
        from handlers.fault_handler import enable_network_fault
        
        network_fault_subtype = data.get('network_fault_subtype', 'timeout')
        target_endpoint = data.get('target_endpoint')
        probability = data.get('probability', 100)
        parameters = data.get('parameters', {})
        
        # Enable fault injection in fault_handler
        success, message = enable_network_fault(
            fault_type=network_fault_subtype,
            target_endpoint=target_endpoint,
            probability=probability,
            **parameters
        )
        
        if success:
            # Update statistics
            fault_statistics['total_injected'] += 1
            fault_statistics['local_faults'] += 1
            
            # Save to database
            Database.save_fault_injection(
                device_id=None,  # Network faults affect all devices
                fault_type='network',
                backend='local_flask',
                error_type=network_fault_subtype,
                success=True,
                parameters=parameters
            )
            
            logger.info(f"[Fault] Network fault injected: {network_fault_subtype}")
            
            return jsonify({
                'success': True,
                'message': message,
                'fault_type': 'network',
                'network_fault_subtype': network_fault_subtype,
                'target_endpoint': target_endpoint,
                'probability': probability,
                'parameters': parameters,
                'backend': 'local_flask/fault_handler'
            }), 201
        else:
            return jsonify({
                'success': False,
                'error': message
            }), 400
        
    except Exception as e:
        logger.error(f"[Fault] Error injecting network fault: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


def inject_ota_fault(data):
    """
    Inject OTA-specific fault using OTA handler (Milestone 5)
    
    Supported OTA Fault Types (triggers ESP32 rollback):
        - corrupt_chunk: Corrupt firmware chunk data (CRC/hash validation fails)
        - bad_hash: Incorrect SHA256 hash in manifest (verification fails)
        - bad_signature: Incorrect signature in manifest (signature check fails)
    
    Request Body:
    {
      "fault_type": "ota",
      "ota_fault_subtype": "corrupt_chunk",   # corrupt_chunk, bad_hash, or bad_signature
      "target_device": "ESP32_001",           # Optional - target specific device
      "target_chunk": 5                       # Optional - for corrupt_chunk only
    }
    """
    try:
        from handlers.ota_handler import enable_ota_fault_injection, VALID_OTA_FAULT_TYPES
        
        ota_fault_subtype = data.get('ota_fault_subtype', 'corrupt_chunk')
        target_device = data.get('target_device')
        target_chunk = data.get('target_chunk')
        
        # Validate fault subtype
        if ota_fault_subtype not in VALID_OTA_FAULT_TYPES:
            return jsonify({
                'success': False,
                'error': f'Invalid OTA fault subtype. Must be one of: {VALID_OTA_FAULT_TYPES}'
            }), 400
        
        # Enable fault injection in OTA handler
        success, message = enable_ota_fault_injection(
            fault_type=ota_fault_subtype,
            target_device=target_device,
            target_chunk=target_chunk
        )
        
        if success:
            # Update statistics
            fault_statistics['total_injected'] += 1
            fault_statistics['local_faults'] += 1
            
            # Save to database with actual OTA fault type (e.g., corrupt_chunk, bad_hash, bad_signature)
            try:
                Database.save_fault_injection(
                    device_id=target_device,
                    fault_type=f'ota_{ota_fault_subtype}',  # Save as ota_corrupt_chunk, ota_bad_hash, etc.
                    backend='local_flask',
                    error_type=ota_fault_subtype,
                    success=True
                )
            except Exception as db_err:
                logger.warning(f"[Fault] Could not save to database: {db_err}")
            
            logger.info(f"[Fault] OTA fault injected: {ota_fault_subtype}")
            
            return jsonify({
                'success': True,
                'message': message,
                'fault_type': f'ota_{ota_fault_subtype}',  # Return specific fault type
                'ota_fault_subtype': ota_fault_subtype,
                'target_device': target_device,
                'target_chunk': target_chunk,
                'backend': 'local_flask/ota_handler'
            }), 201
        else:
            return jsonify({
                'success': False,
                'error': message
            }), 400
        
    except Exception as e:
        logger.error(f"[Fault] Error injecting OTA fault: {e}")
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
        logger.error(f"✗ Error getting fault status: {e}")
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
        logger.error(f"✗ Error clearing faults: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/network/clear', methods=['POST', 'DELETE'])
def clear_network_faults():
    """
    Clear/disable network fault injection
    
    This endpoint disables any active network fault injection.
    Network faults (timeout, disconnect, slow) are applied via Flask middleware
    to ESP32-related endpoints only:
        - /aggregated/*
        - /power/*
        - /ota/*
        - /device/*
        - /command/*
    """
    try:
        from handlers.fault_handler import disable_network_fault, get_network_fault_status
        
        # Get status before clearing
        status_before = get_network_fault_status()
        was_enabled = status_before.get('enabled', False)
        fault_type = status_before.get('fault_type')
        faults_injected = status_before.get('faults_injected', 0)
        
        # Clear network faults
        success, message = disable_network_fault()
        
        if success:
            logger.info(f"✓ Network fault injection cleared (was: {fault_type}, injected: {faults_injected})")
            return jsonify({
                'success': True,
                'message': message,
                'was_enabled': was_enabled,
                'fault_type_cleared': fault_type,
                'faults_injected': faults_injected
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': message
            }), 500
        
    except Exception as e:
        logger.error(f"✗ Error clearing network faults: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/network/status', methods=['GET'])
def get_network_fault_status_route():
    """
    Get current network fault injection status
    
    Returns information about active network faults including:
        - enabled: Whether network fault injection is active
        - fault_type: Type of fault (timeout, disconnect, slow)
        - target_endpoint: Endpoint pattern being targeted (optional)
        - probability: Probability of fault occurring per request
        - parameters: Additional parameters (timeout_ms, delay_ms)
        - faults_injected: Count of faults injected
        - requests_processed: Count of requests processed
        - statistics: Detailed stats by fault type
    """
    try:
        from handlers.fault_handler import get_network_fault_status
        
        status = get_network_fault_status()
        
        return jsonify({
            'success': True,
            'network_fault': status,
            'esp32_endpoints': ['/aggregated', '/power', '/ota', '/device', '/command'],
            'note': 'Network faults only apply to ESP32-related endpoints'
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting network fault status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/types', methods=['GET'])
def get_available_fault_types():
    """
    Get list of available fault types and their descriptions (Milestone 5)
    """
    try:
        fault_types = {
            'inverter_sim': {
                'backend': 'Inverter SIM API (http://20.15.114.131:8080)',
                'endpoint': 'POST /api/user/error-flag/add',
                'description': 'Sets error flag for NEXT ESP32 Modbus request',
                'types': {
                    'EXCEPTION': {
                        'description': 'Modbus exception with exception code',
                        'codes': {
                            '01': 'Illegal Function',
                            '02': 'Illegal Data Address',
                            '03': 'Illegal Data Value',
                            '04': 'Slave Device Failure',
                            '05': 'Acknowledge',
                            '06': 'Slave Device Busy',
                            '08': 'Memory Parity Error',
                            '0A': 'Gateway Path Unavailable',
                            '0B': 'Gateway Target Device Failed to Respond'
                        }
                    },
                    'CRC_ERROR': 'CRC checksum error in response frame',
                    'CORRUPT': 'Corrupted/malformed response data',
                    'PACKET_DROP': 'Dropped packet (no response)',
                    'DELAY': 'Response delay in milliseconds'
                },
                'example': {
                    'errorType': 'EXCEPTION',
                    'exceptionCode': 2,
                    'delayMs': 0
                }
            },
            'local_flask': {
                'backend': 'Local Flask Server',
                'types': {
                    'ota': {
                        'description': 'OTA update faults (triggers ESP32 rollback)',
                        'subtypes': {
                            'corrupt_chunk': 'Corrupt firmware chunk data - ESP32 detects CRC/hash mismatch',
                            'bad_hash': 'Incorrect SHA256 hash in manifest - ESP32 detects hash verification failure',
                            'bad_signature': 'Incorrect signature in manifest - ESP32 detects signature verification failure'
                        },
                        'example': {
                            'fault_type': 'ota',
                            'ota_fault_subtype': 'corrupt_chunk',
                            'target_device': 'ESP32_001',
                            'target_chunk': 5
                        }
                    },
                    'network': {
                        'description': 'Network connectivity issues - Applied via Flask middleware to intercept requests',
                        'subtypes': {
                            'timeout': 'Connection timeout (delay then fail with 504 Gateway Timeout)',
                            'disconnect': 'Connection drop (immediate 503 Service Unavailable)',
                            'slow': 'Slow network speed (add configurable delay to responses without error)'
                        },
                        'parameters': {
                            'timeout_ms': 'Timeout duration for timeout fault (default: 30000ms)',
                            'delay_ms': 'Delay duration for slow fault (default: 3000ms)',
                            'probability': 'Probability of fault occurring per request (0-100%, default: 100)'
                        },
                        'example': {
                            'fault_type': 'network',
                            'network_fault_subtype': 'timeout',
                            'target_endpoint': '/power/upload',
                            'probability': 50,
                            'parameters': {
                                'timeout_ms': 5000
                            }
                        }
                    },
                    'command': {
                        'description': 'Command execution faults',
                        'subtypes': {
                            'timeout': 'Command execution timeout',
                            'invalid_response': 'Return invalid response',
                            'execution_failure': 'Simulate execution failure'
                        },
                        'example': {
                            'fault_type': 'command',
                            'target': 'config_update',
                            'duration': 30
                        }
                    }
                }
            }
        }
        
        return jsonify({
            'success': True,
            'fault_types': fault_types,
            'note': 'Use POST /fault/inject to inject faults'
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting fault types: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/history', methods=['GET'])
def get_fault_history():
    """Get fault injection history from database"""
    try:
        from database import Database
        
        device_id = request.args.get('device_id')
        limit = request.args.get('limit', default=100, type=int)
        
        history = Database.get_fault_history(device_id=device_id, limit=limit)
        
        return jsonify({
            'success': True,
            'history': history,
            'count': len(history)
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting fault history: {e}")
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
      "details": "CRC validation failed, retried successfully after 1 attempt",
      "retry_count": 1                     # Optional: number of retry attempts
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
        
        # Save to database (PERSISTENT STORAGE)
        event_id = Database.save_recovery_event(
            device_id=device_id,
            timestamp=data['timestamp'],
            fault_type=data['fault_type'],
            recovery_action=data['recovery_action'],
            success=data['success'],
            details=data.get('details', ''),
            retry_count=data.get('retry_count', 0)
        )
        
        # Also keep in memory for backward compatibility
        if device_id not in recovery_events:
            recovery_events[device_id] = []
        
        recovery_event = {
            'id': event_id,
            'device_id': device_id,
            'timestamp': data['timestamp'],
            'fault_type': data['fault_type'],
            'recovery_action': data['recovery_action'],
            'success': data['success'],
            'details': data.get('details', ''),
            'retry_count': data.get('retry_count', 0),
            'received_at': int(time.time())
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
        logger.error(f"✗ Error receiving recovery event: {e}", exc_info=True)
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
    
    Response:
    {
      "device_id": "ESP32_EcoWatt_Smart",
      "total_events": 10,
      "events": [
        {
          "id": 1,
          "timestamp": 1698527500,
          "fault_type": "crc_error",
          "recovery_action": "retry_read",
          "success": true,
          "details": "...",
          "retry_count": 1,
          "received_at": "2025-11-05T12:30:00"
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
        
        # Get events from database
        events = Database.get_recovery_events(device_id=device_id, limit=limit)
        
        # Get statistics from database
        stats = Database.get_recovery_statistics(device_id=device_id)
        
        return jsonify({
            'device_id': device_id,
            'total_events': stats['total'],
            'events': events,
            'statistics': {
                'total': stats['total'],
                'successful': stats['successful'],
                'failed': stats['failed'],
                'success_rate': round(stats['success_rate'], 2)
            }
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting recovery history: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/recovery/all', methods=['GET'])
def get_all_recovery_events():
    """
    Get recovery events from all devices
    
    Query Parameters:
    - limit: Maximum number of events to return (default: 100)
    
    Response:
    {
      "events": [
        {
          "id": 1,
          "device_id": "ESP32_EcoWatt_Smart",
          "timestamp": 1698527500,
          "fault_type": "crc_error",
          "recovery_action": "retry_read",
          "success": 1,
          "details": "...",
          "retry_count": 1,
          "received_at": "2025-11-05T12:30:00"
        },
        ...
      ],
      "global_statistics": {
        "total": 20,
        "successful": 16,
        "failed": 4,
        "success_rate": 80.0
      }
    }
    """
    try:
        limit = request.args.get('limit', 100, type=int)
        
        # Get all events from database
        events = Database.get_recovery_events(device_id=None, limit=limit)
        
        # Get global statistics
        stats = Database.get_recovery_statistics(device_id=None)
        
        return jsonify({
            'events': events,
            'global_statistics': {
                'total': stats['total'],
                'successful': stats['successful'],
                'failed': stats['failed'],
                'success_rate': round(stats['success_rate'], 2)
            }
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting all recovery events: {e}")
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
        logger.error(f"✗ Error clearing recovery events: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@fault_bp.route('/fault/injection/history', methods=['GET'])
def get_injection_history():
    """
    Get fault injection history from database
    
    Query Parameters:
    - device_id: Filter by device (optional)
    - limit: Number of records to return (default: 50)
    """
    try:
        device_id = request.args.get('device_id')
        limit = int(request.args.get('limit', 50))
        
        # Get injection history from database
        injections = Database.get_fault_injection_history(device_id, limit)
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'total_injections': len(injections),
            'injections': injections
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting injection history: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
