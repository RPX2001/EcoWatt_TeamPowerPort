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
    'inject_network_fault'
]
