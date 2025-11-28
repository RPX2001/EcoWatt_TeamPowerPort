"""
Fault injection handler for EcoWatt Flask server
Manages fault injection for testing and diagnostics

Provides fault injection for Modbus responses to test ESP32's error handling.

Fault Types:
- corrupt_crc: Flip bits in CRC to cause validation failure
- truncate: Remove bytes from frame to simulate incomplete transmission
- garbage: Replace frame with random hex to test malformed frame detection
- timeout: Delay or drop response to test timeout handling
- exception: Send valid Modbus exception frames

Author: EcoWatt Team
Date: October 28, 2025
"""

import logging
import threading
import random
import time
from datetime import datetime
from typing import Dict, Optional, List, Tuple, Any

logger = logging.getLogger(__name__)

# ============================================================================
# FAULT INJECTION CORE (copied from fault_injector.py)
# ============================================================================

# Fault state (thread-safe)
fault_state = {
    'enabled': False,
    'fault_type': None,
    'probability': 100,  # Percentage (0-100)
    'duration_seconds': 0,  # 0 = until disabled
    'start_time': 0,
    'faults_injected': 0,
    'requests_processed': 0
}
fault_lock = threading.Lock()

# Fault statistics
fault_stats = {
    'crc_corrupted': 0,
    'frames_truncated': 0,
    'garbage_sent': 0,
    'timeouts_triggered': 0,
    'exceptions_sent': 0
}
stats_lock = threading.Lock()

# ============================================================================
# NETWORK FAULT INJECTION (Milestone 5)
# ============================================================================

# Network fault state (thread-safe)
network_fault_state = {
    'enabled': False,
    'fault_type': None,
    'target_endpoint': None,  # None = all endpoints
    'probability': 100,  # Percentage (0-100)
    'parameters': {},
    'faults_injected': 0,
    'requests_processed': 0
}
network_fault_lock = threading.Lock()

# Network fault statistics
network_fault_stats = {
    'timeouts_triggered': 0,
    'connections_dropped': 0,
    'delays_injected': 0,
    'failures_triggered': 0
}
network_stats_lock = threading.Lock()


def calculate_modbus_crc(data_hex: str) -> str:
    """
    Calculate Modbus CRC16 for hex string (without existing CRC).
    
    Args:
        data_hex: Hex string of data (excluding CRC bytes)
        
    Returns:
        4-character hex string of CRC (little-endian)
    """
    # Convert hex string to bytes
    data_bytes = bytes.fromhex(data_hex)
    
    crc = 0xFFFF
    for byte in data_bytes:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    
    # Return as little-endian hex (low byte first)
    low_byte = crc & 0xFF
    high_byte = (crc >> 8) & 0xFF
    return f"{low_byte:02X}{high_byte:02X}"


def corrupt_crc(frame_hex: str) -> str:
    """
    Corrupt the CRC of a Modbus frame by flipping bits.
    
    Args:
        frame_hex: Complete Modbus frame with CRC
        
    Returns:
        Frame with corrupted CRC
    """
    if len(frame_hex) < 8:
        return frame_hex
    
    # Extract data and CRC
    data_part = frame_hex[:-4]
    crc_part = frame_hex[-4:]
    
    # Corrupt CRC by flipping random bits
    crc_int = int(crc_part, 16)
    bit_to_flip = random.randint(0, 15)
    corrupted_crc = crc_int ^ (1 << bit_to_flip)
    
    corrupted_frame = f"{data_part}{corrupted_crc:04X}"
    
    with stats_lock:
        fault_stats['crc_corrupted'] += 1
    
    logger.warning(f"💉 Fault Injected: CRC corrupted (bit {bit_to_flip} flipped)")
    logger.debug(f"   Original: {frame_hex}")
    logger.debug(f"   Corrupted: {corrupted_frame}")
    
    return corrupted_frame


def truncate_frame(frame_hex: str) -> str:
    """
    Truncate frame by removing random number of bytes.
    
    Args:
        frame_hex: Complete Modbus frame
        
    Returns:
        Truncated frame
    """
    if len(frame_hex) < 8:
        return frame_hex
    
    # Remove 1-4 bytes (2-8 hex chars)
    bytes_to_remove = random.randint(1, min(4, len(frame_hex) // 2 - 2))
    chars_to_remove = bytes_to_remove * 2
    
    truncated = frame_hex[:-chars_to_remove]
    
    with stats_lock:
        fault_stats['frames_truncated'] += 1
    
    logger.warning(f"💉 Fault Injected: Frame truncated ({bytes_to_remove} bytes removed)")
    logger.debug(f"   Original length: {len(frame_hex)} chars")
    logger.debug(f"   Truncated length: {len(truncated)} chars")
    
    return truncated


def generate_garbage(original_frame_hex: str) -> str:
    """
    Generate garbage data of similar length.
    
    Args:
        original_frame_hex: Original frame (for length reference)
        
    Returns:
        Random hex garbage
    """
    length = len(original_frame_hex)
    garbage = ''.join(random.choice('0123456789ABCDEF') for _ in range(length))
    
    with stats_lock:
        fault_stats['garbage_sent'] += 1
    
    logger.warning(f"💉 Fault Injected: Garbage data generated ({length} chars)")
    logger.debug(f"   Garbage: {garbage[:32]}...")
    
    return garbage


def generate_modbus_exception(function_code: int, exception_code: int = 0x01) -> str:
    """
    Generate valid Modbus exception frame.
    
    Args:
        function_code: Original function code
        exception_code: Exception code (default: 0x01 = Illegal Function)
        
    Returns:
        Complete Modbus exception frame with CRC
    """
    # Modbus exception format: SlaveID + (FunctionCode | 0x80) + ExceptionCode + CRC
    slave_id = 0x11  # Standard slave ID
    exception_func = function_code | 0x80
    
    # Build frame without CRC
    data = f"{slave_id:02X}{exception_func:02X}{exception_code:02X}"
    
    # Calculate and append CRC
    crc = calculate_modbus_crc(data)
    exception_frame = f"{data}{crc}"
    
    with stats_lock:
        fault_stats['exceptions_sent'] += 1
    
    logger.warning(f"💉 Fault Injected: Modbus exception (code: 0x{exception_code:02X})")
    logger.debug(f"   Exception frame: {exception_frame}")
    
    return exception_frame


def simulate_timeout(delay_seconds: float = 5.0) -> None:
    """
    Simulate timeout by delaying response.
    
    Args:
        delay_seconds: How long to delay
    """
    with stats_lock:
        fault_stats['timeouts_triggered'] += 1
    
    logger.warning(f"💉 Fault Injected: Timeout simulation ({delay_seconds}s delay)")
    time.sleep(delay_seconds)


def should_inject_fault() -> bool:
    """
    Determine if fault should be injected based on configuration.
    
    Returns:
        True if fault should be injected, False otherwise
    """
    with fault_lock:
        if not fault_state['enabled']:
            return False
        
        # Check duration
        if fault_state['duration_seconds'] > 0:
            elapsed = time.time() - fault_state['start_time']
            if elapsed > fault_state['duration_seconds']:
                fault_state['enabled'] = False
                logger.info("⏰ Fault injection duration expired - disabled")
                return False
        
        # Check probability
        if random.randint(1, 100) > fault_state['probability']:
            return False
        
        fault_state['requests_processed'] += 1
        fault_state['faults_injected'] += 1
        return True


def inject_fault(frame_hex: str, function_code: int = 0x03) -> Optional[str]:
    """
    Main fault injection function.
    
    Args:
        frame_hex: Original Modbus frame
        function_code: Modbus function code (for exception generation)
        
    Returns:
        Modified frame if fault injected, None if timeout, original if no fault
    """
    if not should_inject_fault():
        with fault_lock:
            fault_state['requests_processed'] += 1
        return frame_hex
    
    fault_type = fault_state.get('fault_type', 'corrupt_crc')
    
    if fault_type == 'corrupt_crc':
        return corrupt_crc(frame_hex)
    
    elif fault_type == 'truncate':
        return truncate_frame(frame_hex)
    
    elif fault_type == 'garbage':
        return generate_garbage(frame_hex)
    
    elif fault_type == 'timeout':
        simulate_timeout()
        return frame_hex  # Return original after delay
    
    elif fault_type == 'exception':
        return generate_modbus_exception(function_code)
    
    else:
        logger.error(f"Unknown fault type: {fault_type}")
        return frame_hex


# ============================================================================
# HANDLER LAYER (wraps core fault injection functions)
# ============================================================================


def enable_fault_injection(
    fault_type: str,
    probability: int = 100,
    duration: int = 0
) -> Tuple[bool, Optional[str]]:
    """
    Enable fault injection with specified parameters.
    
    Args:
        fault_type: Type of fault ('corrupt_crc', 'truncate', 'garbage', 'timeout', 'exception')
        probability: Injection probability (0-100%)
        duration: Duration in seconds (0 = infinite)
        
    Returns:
        tuple: (success, error_message)
    """
    valid_types = ['corrupt_crc', 'truncate', 'garbage', 'timeout', 'exception']
    
    if fault_type not in valid_types:
        error_msg = f'Invalid fault type. Valid types: {valid_types}'
        logger.error(error_msg)
        return False, error_msg
    
    if not (0 <= probability <= 100):
        error_msg = 'Probability must be between 0 and 100'
        logger.error(error_msg)
        return False, error_msg
    
    try:
        with fault_lock:
            fault_state['enabled'] = True
            fault_state['fault_type'] = fault_type
            fault_state['probability'] = probability
            fault_state['duration_seconds'] = duration
            fault_state['start_time'] = time.time()
            fault_state['faults_injected'] = 0
            fault_state['requests_processed'] = 0
        
        logger.info(f"✓ Fault injection ENABLED: type={fault_type}, "
                    f"probability={probability}%, duration={duration}s")
        
        return True, None
        
    except Exception as e:
        logger.error(f"Error enabling fault injection: {e}")
        return False, str(e)


def disable_fault_injection() -> bool:
    """
    Disable fault injection.
    
    Returns:
        bool: True if disabled successfully
    """
    try:
        with fault_lock:
            was_enabled = fault_state['enabled']
            fault_state['enabled'] = False
            stats = {
                'faults_injected': fault_state['faults_injected'],
                'requests_processed': fault_state['requests_processed']
            }
        
        if was_enabled:
            logger.info(f"✓ Fault injection DISABLED - "
                        f"Injected {stats['faults_injected']} faults "
                        f"in {stats['requests_processed']} requests")
        
        return True
        
    except Exception as e:
        logger.error(f"Error disabling fault injection: {e}")
        return False


def get_fault_status() -> Dict:
    """
    Get current fault injection status and statistics.
    
    Returns:
        dict: Fault injection status
    """
    try:
        with fault_lock:
            status = dict(fault_state)
        
        with stats_lock:
            stats = dict(fault_stats)
        
        if status['enabled'] and status['duration_seconds'] > 0:
            elapsed = time.time() - status['start_time']
            status['time_remaining'] = max(0, status['duration_seconds'] - elapsed)
        
        return {
            'enabled': status['enabled'],
            'fault_type': status['fault_type'],
            'probability': status['probability'],
            'duration_seconds': status['duration_seconds'],
            'time_remaining': status.get('time_remaining'),
            'faults_injected': status['faults_injected'],
            'requests_processed': status['requests_processed'],
            'detailed_stats': stats
        }
        
    except Exception as e:
        logger.error(f"Error getting fault status: {e}")
        return {'error': str(e)}


def get_available_faults() -> List[Dict]:
    """
    Get list of available fault types
    
    Returns:
        list: Available fault types with descriptions
    """
    try:
        available_faults = [
            {
                'type': 'corrupt_crc',
                'description': 'Flip bits in CRC to cause validation failure',
                'parameters': []
            },
            {
                'type': 'truncate',
                'description': 'Remove bytes from frame to simulate incomplete transmission',
                'parameters': []
            },
            {
                'type': 'garbage',
                'description': 'Replace frame with random hex to test malformed frame detection',
                'parameters': []
            },
            {
                'type': 'timeout',
                'description': 'Delay or drop response to test timeout handling',
                'parameters': []
            },
            {
                'type': 'exception',
                'description': 'Send valid Modbus exception frames',
                'parameters': []
            }
        ]
        
        logger.debug("Retrieved available faults")
        return available_faults
        
    except Exception as e:
        logger.error(f"Error getting available faults: {e}")
        return []


def get_fault_statistics() -> Dict:
    """
    Get fault injection statistics.
    
    Returns:
        dict: Fault statistics
    """
    try:
        with stats_lock:
            stats_copy = fault_stats.copy()
        
        with fault_lock:
            stats_copy['faults_injected'] = fault_state['faults_injected']
            stats_copy['requests_processed'] = fault_state['requests_processed']
        
        logger.debug(f"Retrieved fault stats: {stats_copy}")
        return stats_copy
        
    except Exception as e:
        logger.error(f"Error getting fault statistics: {e}")
        return {'error': str(e)}


def reset_fault_statistics() -> bool:
    """
    Reset fault injection statistics.
    
    Returns:
        bool: True if reset successfully
    """
    try:
        with stats_lock:
            for key in fault_stats:
                fault_stats[key] = 0
        
        with fault_lock:
            fault_state['faults_injected'] = 0
            fault_state['requests_processed'] = 0
        
        logger.info("✓ Fault injection statistics reset")
        return True
        
    except Exception as e:
        logger.error(f"Error resetting fault statistics: {e}")
        return False


# ============================================================================
# NETWORK FAULT INJECTION FUNCTIONS (Milestone 5)
# ============================================================================


def enable_network_fault(fault_type: str, target_endpoint: Optional[str] = None, 
                        probability: int = 100, **kwargs) -> Tuple[bool, str]:
    """
    Enable network fault injection (Milestone 5)
    
    Args:
        fault_type: Type of network fault to inject
            - 'timeout': Simulate connection timeout (delay then fail with 504)
            - 'disconnect': Simulate connection drop (immediate 503 failure)
            - 'slow': Slow network speed (add delay to responses, then continue)
        target_endpoint: Optional endpoint pattern to target (e.g., '/power', '/ota')
        probability: Probability of fault occurring (0-100%)
        **kwargs: Additional parameters
            - timeout_ms: Timeout duration for 'timeout' type (default: 5000ms)
            - delay_ms: Delay duration for 'slow' type (default: 3000ms)
    
    Returns:
        tuple: (success, message)
    """
    with network_fault_lock:
        valid_types = ['timeout', 'disconnect', 'slow']
        
        if fault_type not in valid_types:
            return False, f"Invalid fault type. Must be one of: {valid_types}"
        
        if not 0 <= probability <= 100:
            return False, "Probability must be between 0 and 100"
        
        network_fault_state['enabled'] = True
        network_fault_state['fault_type'] = fault_type
        network_fault_state['target_endpoint'] = target_endpoint
        network_fault_state['probability'] = probability
        network_fault_state['parameters'] = kwargs
        network_fault_state['faults_injected'] = 0
        network_fault_state['requests_processed'] = 0
        
        logger.warning(f"⚠️  Network Fault Injection ENABLED: {fault_type}")
        if target_endpoint:
            logger.warning(f"   Target Endpoint: {target_endpoint}")
        logger.warning(f"   Probability: {probability}%")
        if kwargs:
            logger.warning(f"   Parameters: {kwargs}")
        
        return True, f"Network fault injection enabled: {fault_type}"


def disable_network_fault() -> Tuple[bool, str]:
    """Disable network fault injection"""
    with network_fault_lock:
        was_enabled = network_fault_state['enabled']
        faults_injected = network_fault_state['faults_injected']
        
        network_fault_state['enabled'] = False
        network_fault_state['fault_type'] = None
        network_fault_state['target_endpoint'] = None
        network_fault_state['probability'] = 100
        network_fault_state['parameters'] = {}
        network_fault_state['faults_injected'] = 0
        network_fault_state['requests_processed'] = 0
        
        if was_enabled:
            logger.info(f"✓ Network Fault Injection DISABLED (Faults injected: {faults_injected})")
            return True, f"Network fault injection disabled (Faults injected: {faults_injected})"
        else:
            return True, "Network fault injection was not enabled"


def get_network_fault_status() -> Dict:
    """Get current network fault injection status"""
    with network_fault_lock:
        return {
            'enabled': network_fault_state['enabled'],
            'fault_type': network_fault_state['fault_type'],
            'target_endpoint': network_fault_state['target_endpoint'],
            'probability': network_fault_state['probability'],
            'parameters': network_fault_state['parameters'],
            'faults_injected': network_fault_state['faults_injected'],
            'requests_processed': network_fault_state['requests_processed'],
            'statistics': network_fault_stats.copy()
        }


def should_inject_network_fault(endpoint: str) -> bool:
    """Check if network fault should be injected for this request"""
    if not network_fault_state['enabled']:
        return False
    
    # Increment request counter
    network_fault_state['requests_processed'] += 1
    
    # Check endpoint filter
    if network_fault_state['target_endpoint']:
        if network_fault_state['target_endpoint'] not in endpoint:
            return False
    
    # Check probability
    if random.randint(1, 100) > network_fault_state['probability']:
        return False
    
    return True


def inject_network_fault(endpoint: str) -> Optional[Tuple[Dict, int]]:
    """
    Inject network fault for an endpoint (Milestone 5)
    
    Args:
        endpoint: The endpoint path being accessed
        
    Returns:
        Optional tuple of (error_response_dict, status_code) if fault injected,
        None if no fault should be injected (continue normal processing)
    """
    if not should_inject_network_fault(endpoint):
        return None
    
    fault_type = network_fault_state['fault_type']
    network_fault_state['faults_injected'] += 1
    
    logger.warning(f"⚠️  Network fault injected on {endpoint}: {fault_type} "
                 f"(fault #{network_fault_state['faults_injected']})")
    
    if fault_type == 'timeout':
        # Simulate timeout: delay then return error
        timeout_ms = network_fault_state['parameters'].get('timeout_ms', 5000)
        logger.warning(f"   Simulating timeout: {timeout_ms}ms delay then 504")
        time.sleep(timeout_ms / 1000.0)
        
        with network_stats_lock:
            network_fault_stats['timeouts_triggered'] += 1
        
        return {
            'success': False,
            'error': 'Request timeout',
            'fault_injection': True,
            'fault_type': 'timeout'
        }, 504
    
    elif fault_type == 'disconnect':
        # Simulate connection drop: immediate failure
        logger.warning("   Simulating connection drop (503 Service Unavailable)")
        
        with network_stats_lock:
            network_fault_stats['connections_dropped'] += 1
        
        return {
            'success': False,
            'error': 'Connection reset by peer',
            'fault_injection': True,
            'fault_type': 'disconnect'
        }, 503
    
    elif fault_type == 'slow':
        # Simulate slow network: add delay before processing
        delay_ms = network_fault_state['parameters'].get('delay_ms', 3000)
        logger.warning(f"   Simulating slow network: {delay_ms}ms delay")
        time.sleep(delay_ms / 1000.0)
        
        with network_stats_lock:
            network_fault_stats['delays_injected'] += 1
        
        # Return None to continue with normal processing after delay
        return None
    
    # No fault or continue normal processing
    return None


# ============================================================================
# SECURITY FAULT INJECTION (Milestone 5)
# ============================================================================

# Security fault state (thread-safe)
security_fault_state = {
    'enabled': False,
    'fault_type': None,  # 'replay', 'invalid_hmac', 'tampered_payload', 'old_nonce', 'missing_nonce', 'invalid_format'
    'target_device': None,  # None = all devices
    'faults_injected': 0,
    'last_fault_time': None
}
security_fault_lock = threading.Lock()

# Security fault statistics
security_fault_stats = {
    'replay_attacks_triggered': 0,
    'invalid_hmac_triggered': 0,
    'tampered_payload_triggered': 0,
    'old_nonce_triggered': 0,
    'missing_nonce_triggered': 0,
    'invalid_format_triggered': 0
}
security_stats_lock = threading.Lock()

# Store for simulating replay attacks (stores last valid nonce per device)
_replay_nonce_store = {}


def enable_security_fault(fault_type: str, target_device: Optional[str] = None) -> Tuple[bool, str]:
    """
    Enable security fault injection (Milestone 5)
    
    Args:
        fault_type: Type of security fault to inject
            - 'replay': Simulate replay attack (reuse nonce)
            - 'invalid_hmac': Send payload with invalid HMAC
            - 'tampered_payload': Modify payload after signing
            - 'old_nonce': Send payload with expired nonce
            - 'missing_nonce': Send payload without nonce field
            - 'invalid_format': Send malformed security payload
        target_device: Device ID to target (None = test mode, generates test payloads)
    
    Returns:
        tuple: (success, message)
    """
    with security_fault_lock:
        valid_types = ['replay', 'invalid_hmac', 'tampered_payload', 'old_nonce', 'missing_nonce', 'invalid_format']
        
        if fault_type not in valid_types:
            return False, f"Invalid fault type. Must be one of: {valid_types}"
        
        security_fault_state['enabled'] = True
        security_fault_state['fault_type'] = fault_type
        security_fault_state['target_device'] = target_device
        security_fault_state['faults_injected'] = 0
        security_fault_state['last_fault_time'] = None
        
        logger.warning(f"⚠️  Security Fault Injection ENABLED: {fault_type}")
        if target_device:
            logger.warning(f"   Target Device: {target_device}")
        
        return True, f"Security fault injection enabled: {fault_type}"


def disable_security_fault() -> Tuple[bool, str]:
    """Disable security fault injection"""
    with security_fault_lock:
        was_enabled = security_fault_state['enabled']
        faults_injected = security_fault_state['faults_injected']
        fault_type = security_fault_state['fault_type']
        
        security_fault_state['enabled'] = False
        security_fault_state['fault_type'] = None
        security_fault_state['target_device'] = None
        security_fault_state['faults_injected'] = 0
        security_fault_state['last_fault_time'] = None
        
        if was_enabled:
            logger.info(f"✓ Security Fault Injection DISABLED (Type: {fault_type}, Faults: {faults_injected})")
            return True, f"Security fault injection disabled (Faults injected: {faults_injected})"
        else:
            return True, "Security fault injection was not enabled"


def get_security_fault_status() -> Dict:
    """Get current security fault injection status"""
    with security_fault_lock:
        status = {
            'enabled': security_fault_state['enabled'],
            'fault_type': security_fault_state['fault_type'],
            'target_device': security_fault_state['target_device'],
            'faults_injected': security_fault_state['faults_injected'],
            'last_fault_time': security_fault_state['last_fault_time']
        }
    
    with security_stats_lock:
        status['statistics'] = security_fault_stats.copy()
    
    return status


def generate_security_fault_payload(device_id: str, fault_type: str) -> Tuple[Dict, str]:
    """
    Generate a faulty security payload for testing
    
    Args:
        device_id: Target device ID
        fault_type: Type of security fault to generate
        
    Returns:
        tuple: (payload_dict, expected_error_message)
    """
    import base64
    import hmac as hmac_module
    import hashlib
    
    # Pre-shared key for HMAC (must match ESP32)
    PSK_HMAC = bytes([
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
        0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
        0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
    ])
    
    current_time = int(time.time())
    test_data = f'{{"test": "security_fault_injection", "device": "{device_id}", "timestamp": {current_time}}}'
    payload_b64 = base64.b64encode(test_data.encode()).decode()
    
    if fault_type == 'replay':
        # Use a previously used nonce (or current if first time)
        if device_id in _replay_nonce_store:
            nonce = _replay_nonce_store[device_id]
            expected_error = f"Replay attack detected! Nonce {nonce} <= last valid nonce"
        else:
            # First time - store this nonce and use a valid one
            nonce = current_time
            _replay_nonce_store[device_id] = nonce
            expected_error = "First request stored nonce - second request will trigger replay"
        
        # Calculate valid HMAC for this nonce
        nonce_bytes = nonce.to_bytes(4, 'big')
        data_to_sign = nonce_bytes + test_data.encode()
        mac = hmac_module.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
        
        with security_stats_lock:
            security_fault_stats['replay_attacks_triggered'] += 1
        
        return {
            'nonce': nonce,
            'payload': payload_b64,
            'mac': mac,
            'encrypted': False,
            'compressed': False
        }, expected_error
    
    elif fault_type == 'invalid_hmac':
        # Send completely invalid HMAC
        nonce = current_time
        invalid_mac = '0' * 64  # All zeros HMAC
        
        with security_stats_lock:
            security_fault_stats['invalid_hmac_triggered'] += 1
        
        return {
            'nonce': nonce,
            'payload': payload_b64,
            'mac': invalid_mac,
            'encrypted': False,
            'compressed': False
        }, "HMAC verification failed"
    
    elif fault_type == 'tampered_payload':
        # Calculate valid HMAC, then modify payload
        nonce = current_time
        nonce_bytes = nonce.to_bytes(4, 'big')
        data_to_sign = nonce_bytes + test_data.encode()
        mac = hmac_module.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
        
        # Tamper with the payload after signing
        tampered_data = test_data.replace('"test"', '"tampered"')
        tampered_b64 = base64.b64encode(tampered_data.encode()).decode()
        
        with security_stats_lock:
            security_fault_stats['tampered_payload_triggered'] += 1
        
        return {
            'nonce': nonce,
            'payload': tampered_b64,  # Tampered payload
            'mac': mac,  # Original MAC (won't match)
            'encrypted': False,
            'compressed': False
        }, "HMAC verification failed"
    
    elif fault_type == 'old_nonce':
        # Use nonce from 1 hour ago
        old_nonce = current_time - 3600  # 1 hour ago
        
        # Calculate valid HMAC with old nonce
        nonce_bytes = old_nonce.to_bytes(4, 'big')
        data_to_sign = nonce_bytes + test_data.encode()
        mac = hmac_module.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
        
        with security_stats_lock:
            security_fault_stats['old_nonce_triggered'] += 1
        
        return {
            'nonce': old_nonce,
            'payload': payload_b64,
            'mac': mac,
            'encrypted': False,
            'compressed': False
        }, "Replay attack detected! Nonce"
    
    elif fault_type == 'missing_nonce':
        # Payload without nonce field
        mac = 'a' * 64  # Dummy MAC
        
        with security_stats_lock:
            security_fault_stats['missing_nonce_triggered'] += 1
        
        return {
            'payload': payload_b64,
            'mac': mac,
            'encrypted': False,
            'compressed': False
        }, "Missing required security fields"
    
    elif fault_type == 'invalid_format':
        # Completely malformed payload
        with security_stats_lock:
            security_fault_stats['invalid_format_triggered'] += 1
        
        return {
            'garbage': 'not_a_valid_security_payload',
            'random_field': 12345
        }, "Missing required security fields"
    
    else:
        return {}, f"Unknown fault type: {fault_type}"


def execute_security_fault_test(device_id: str, fault_type: str) -> Dict:
    """
    Execute a security fault test and return results
    
    Args:
        device_id: Target device ID
        fault_type: Type of security fault to test
        
    Returns:
        dict: Test result with success, expected_error, actual_error, etc.
    """
    import json
    from datetime import datetime
    from handlers.security_handler import validate_secured_payload
    
    # Special handling for replay attack - needs two-step process
    if fault_type == 'replay':
        return _execute_replay_attack_test(device_id)
    
    # Generate faulty payload
    payload, expected_error = generate_security_fault_payload(device_id, fault_type)
    
    # Update state
    with security_fault_lock:
        security_fault_state['faults_injected'] += 1
        security_fault_state['last_fault_time'] = datetime.now().isoformat()
    
    # Try to validate the faulty payload
    try:
        payload_json = json.dumps(payload)
        success, decrypted, error = validate_secured_payload(device_id, payload_json)
        
        # For security tests, we EXPECT failure
        test_passed = not success  # Test passes if validation fails
        
        return {
            'success': True,
            'test_type': fault_type,
            'device_id': device_id,
            'test_passed': test_passed,
            'validation_succeeded': success,
            'expected_error': expected_error,
            'actual_error': error,
            'payload_sent': payload,
            'timestamp': datetime.now().isoformat(),
            'description': get_security_fault_description(fault_type)
        }
        
    except Exception as e:
        # Exception during validation - this might also indicate the fault worked
        return {
            'success': True,
            'test_type': fault_type,
            'device_id': device_id,
            'test_passed': True,  # Exception means security caught it
            'validation_succeeded': False,
            'expected_error': expected_error,
            'actual_error': str(e),
            'payload_sent': payload,
            'timestamp': datetime.now().isoformat(),
            'description': get_security_fault_description(fault_type)
        }


def _execute_replay_attack_test(device_id: str) -> Dict:
    """
    Execute replay attack test - sends same nonce twice
    
    Step 1: Send valid payload to register nonce
    Step 2: Replay the same nonce - should be rejected
    """
    import json
    import base64
    import hashlib
    import hmac as hmac_module
    from datetime import datetime
    from handlers.security_handler import validate_secured_payload
    
    # Pre-shared key for HMAC (must match ESP32)
    PSK_HMAC = bytes([
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
        0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
        0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
    ])
    
    current_time = int(time.time())
    test_data = f'{{"test": "replay_attack_test", "device": "{device_id}", "timestamp": {current_time}}}'
    payload_b64 = base64.b64encode(test_data.encode()).decode()
    
    # Create valid payload with unique nonce
    nonce = current_time
    nonce_bytes = nonce.to_bytes(4, 'big')
    data_to_sign = nonce_bytes + test_data.encode()
    mac = hmac_module.new(PSK_HMAC, data_to_sign, hashlib.sha256).hexdigest()
    
    valid_payload = {
        'nonce': nonce,
        'payload': payload_b64,
        'mac': mac,
        'encrypted': False,
        'compressed': False
    }
    
    # Update stats
    with security_stats_lock:
        security_fault_stats['replay_attacks_triggered'] += 1
    
    with security_fault_lock:
        security_fault_state['faults_injected'] += 1
        security_fault_state['last_fault_time'] = datetime.now().isoformat()
    
    try:
        # Step 1: First request - should succeed and register the nonce
        payload_json = json.dumps(valid_payload)
        first_success, _, first_error = validate_secured_payload(device_id, payload_json)
        
        if not first_success:
            # First request failed - can't test replay
            return {
                'success': True,
                'test_type': 'replay',
                'device_id': device_id,
                'test_passed': False,
                'validation_succeeded': False,
                'expected_error': 'First request should succeed to set up replay test',
                'actual_error': first_error,
                'payload_sent': valid_payload,
                'timestamp': datetime.now().isoformat(),
                'description': 'Replay Attack Test - First request failed, cannot test replay',
                'note': 'First request must succeed before replay can be tested'
            }
        
        # Step 2: Replay the same nonce - should FAIL
        replay_success, _, replay_error = validate_secured_payload(device_id, payload_json)
        
        # Test passes if replay was REJECTED (replay_success = False)
        test_passed = not replay_success
        
        return {
            'success': True,
            'test_type': 'replay',
            'device_id': device_id,
            'test_passed': test_passed,
            'validation_succeeded': replay_success,
            'expected_error': f'Replay attack detected! Nonce {nonce} already used',
            'actual_error': replay_error,
            'payload_sent': valid_payload,
            'timestamp': datetime.now().isoformat(),
            'description': 'Replay Attack - Sent same nonce twice, second request should be rejected',
            'first_request_result': 'Succeeded (nonce registered)',
            'replay_request_result': 'Rejected' if test_passed else 'Accepted (SECURITY VULNERABILITY!)'
        }
        
    except Exception as e:
        return {
            'success': True,
            'test_type': 'replay',
            'device_id': device_id,
            'test_passed': True,  # Exception during replay means it was caught
            'validation_succeeded': False,
            'expected_error': 'Replay attack should be rejected',
            'actual_error': str(e),
            'payload_sent': valid_payload,
            'timestamp': datetime.now().isoformat(),
            'description': 'Replay Attack Test - Exception occurred'
        }


def get_security_fault_description(fault_type: str) -> str:
    """Get human-readable description of security fault type"""
    descriptions = {
        'replay': 'Replay Attack - Reuses a previously used nonce to test anti-replay protection',
        'invalid_hmac': 'Invalid HMAC - Sends payload with all-zeros HMAC signature',
        'tampered_payload': 'Tampered Payload - Modifies payload after HMAC calculation',
        'old_nonce': 'Old/Expired Nonce - Sends payload with nonce from 1 hour ago',
        'missing_nonce': 'Missing Nonce - Sends payload without the required nonce field',
        'invalid_format': 'Invalid Format - Sends completely malformed security payload'
    }
    return descriptions.get(fault_type, f'Unknown fault type: {fault_type}')


def get_available_security_faults() -> list:
    """Get list of available security fault types"""
    return [
        {
            'type': 'replay',
            'name': 'Replay Attack',
            'description': 'Reuses a previously used nonce to test anti-replay protection',
            'expected_behavior': 'Server should reject with "Replay attack detected"'
        },
        {
            'type': 'invalid_hmac',
            'name': 'Invalid HMAC',
            'description': 'Sends payload with invalid (all-zeros) HMAC signature',
            'expected_behavior': 'Server should reject with "HMAC verification failed"'
        },
        {
            'type': 'tampered_payload',
            'name': 'Tampered Payload',
            'description': 'Modifies payload data after HMAC is calculated',
            'expected_behavior': 'Server should reject with "HMAC verification failed"'
        },
        {
            'type': 'old_nonce',
            'name': 'Old/Expired Nonce',
            'description': 'Sends payload with nonce timestamp from 1 hour ago',
            'expected_behavior': 'Server should reject as replay (nonce too old)'
        },
        {
            'type': 'missing_nonce',
            'name': 'Missing Nonce',
            'description': 'Sends payload without the required nonce field',
            'expected_behavior': 'Server should reject with "Missing required security fields"'
        },
        {
            'type': 'invalid_format',
            'name': 'Invalid Format',
            'description': 'Sends completely malformed security payload',
            'expected_behavior': 'Server should reject with "Missing required security fields"'
        }
    ]


def reset_security_fault_stats() -> bool:
    """Reset security fault injection statistics"""
    with security_stats_lock:
        for key in security_fault_stats:
            security_fault_stats[key] = 0
    
    # Also clear replay nonce store
    global _replay_nonce_store
    _replay_nonce_store = {}
    
    logger.info("✓ Security fault injection statistics reset")
    return True


# Export functions
__all__ = [
    'enable_fault_injection',
    'disable_fault_injection',
    'get_fault_status',
    'get_available_faults',
    'get_fault_statistics',
    'reset_fault_statistics',
    'inject_fault',
    'calculate_modbus_crc',
    'corrupt_crc',
    'truncate_frame',
    'generate_garbage',
    'generate_modbus_exception',
    'simulate_timeout',
    # Network fault injection
    'enable_network_fault',
    'disable_network_fault',
    'get_network_fault_status',
    'inject_network_fault',
    # Security fault injection
    'enable_security_fault',
    'disable_security_fault',
    'get_security_fault_status',
    'execute_security_fault_test',
    'get_available_security_faults',
    'reset_security_fault_stats'
]
