"""
Fault Injection Module for EcoWatt Flask Server

Provides endpoints and utilities for injecting faults into Modbus responses
to test the ESP32's error handling, retry logic, and diagnostics.

Fault Types:
- corrupt_crc: Flip bits in CRC to cause validation failure
- truncate: Remove bytes from frame to simulate incomplete transmission
- garbage: Replace frame with random hex to test malformed frame detection
- timeout: Delay or drop response to test timeout handling
- exception: Send valid Modbus exception frames

Author: EcoWatt Team
Date: October 22, 2025
"""

import random
import time
from threading import Lock
from typing import Optional, Dict, Any
import logging

logger = logging.getLogger(__name__)

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
fault_lock = Lock()

# Fault statistics
fault_stats = {
    'crc_corrupted': 0,
    'frames_truncated': 0,
    'garbage_sent': 0,
    'timeouts_triggered': 0,
    'exceptions_sent': 0
}
stats_lock = Lock()


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


def enable_fault_injection(fault_type: str, probability: int = 100, 
                          duration: int = 0) -> Dict[str, Any]:
    """
    Enable fault injection with specified parameters.
    
    Args:
        fault_type: Type of fault ('corrupt_crc', 'truncate', 'garbage', 'timeout', 'exception')
        probability: Injection probability (0-100%)
        duration: Duration in seconds (0 = infinite)
        
    Returns:
        Status dictionary
    """
    valid_types = ['corrupt_crc', 'truncate', 'garbage', 'timeout', 'exception']
    
    if fault_type not in valid_types:
        return {
            'success': False,
            'error': f'Invalid fault type. Valid types: {valid_types}'
        }
    
    if not (0 <= probability <= 100):
        return {
            'success': False,
            'error': 'Probability must be between 0 and 100'
        }
    
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
    
    return {
        'success': True,
        'message': 'Fault injection enabled',
        'config': {
            'fault_type': fault_type,
            'probability': probability,
            'duration_seconds': duration
        }
    }


def disable_fault_injection() -> Dict[str, Any]:
    """
    Disable fault injection.
    
    Returns:
        Status dictionary with statistics
    """
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
    
    return {
        'success': True,
        'message': 'Fault injection disabled',
        'statistics': stats
    }


def get_fault_status() -> Dict[str, Any]:
    """
    Get current fault injection status and statistics.
    
    Returns:
        Status dictionary
    """
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


def reset_statistics() -> Dict[str, Any]:
    """
    Reset fault injection statistics.
    
    Returns:
        Confirmation dictionary
    """
    with stats_lock:
        for key in fault_stats:
            fault_stats[key] = 0
    
    with fault_lock:
        fault_state['faults_injected'] = 0
        fault_state['requests_processed'] = 0
    
    logger.info("✓ Fault injection statistics reset")
    
    return {
        'success': True,
        'message': 'Statistics reset'
    }


# Test function
def test_fault_injector():
    """Test the fault injector with sample frame"""
    print("\n=== Fault Injector Test ===\n")
    
    # Sample Modbus frame (read response)
    test_frame = "11031409040000138F066A066300000000016100320000BAAD"
    
    print(f"Original frame: {test_frame}\n")
    
    # Test each fault type
    fault_types = ['corrupt_crc', 'truncate', 'garbage', 'exception', 'timeout']
    
    for fault_type in fault_types:
        enable_fault_injection(fault_type, 100, 0)
        modified = inject_fault(test_frame, 0x03)
        print(f"{fault_type}: {modified}\n")
        disable_fault_injection()
    
    # Print stats
    print("Statistics:")
    status = get_fault_status()
    print(f"  {status['detailed_stats']}\n")


def main():
    """
    CLI interface for fault injection testing.
    
    Usage:
        python fault_injector.py test                        # Run test
        python fault_injector.py enable <type> [options]     # Enable fault
        python fault_injector.py disable                     # Disable fault
        python fault_injector.py status                      # Show status
        python fault_injector.py inject <frame> [func_code]  # Inject fault into frame
    """
    import argparse
    
    parser = argparse.ArgumentParser(
        description='EcoWatt Fault Injector - Modbus fault injection tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Fault Types:
  corrupt_crc  - Flip bits in CRC to cause validation failure
  truncate     - Remove bytes from frame to simulate incomplete transmission
  garbage      - Replace frame with random hex
  timeout      - Delay or drop response
  exception    - Send valid Modbus exception frames

Examples:
  # Run built-in test
  python fault_injector.py test
  
  # Enable CRC corruption at 50% probability for 60 seconds
  python fault_injector.py enable corrupt_crc --probability 50 --duration 60
  
  # Inject fault into a specific frame
  python fault_injector.py inject 11031409040000138F066A0663 --function-code 3
  
  # Check current status
  python fault_injector.py status
  
  # Disable fault injection
  python fault_injector.py disable
        """
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # Test command
    subparsers.add_parser('test', help='Run built-in fault injector test')
    
    # Enable command
    enable_parser = subparsers.add_parser('enable', help='Enable fault injection')
    enable_parser.add_argument('fault_type', 
                              choices=['corrupt_crc', 'truncate', 'garbage', 'timeout', 'exception'],
                              help='Type of fault to inject')
    enable_parser.add_argument('--probability', '-p', type=int, default=100,
                              help='Injection probability 0-100%% (default: 100)')
    enable_parser.add_argument('--duration', '-d', type=int, default=0,
                              help='Duration in seconds, 0=infinite (default: 0)')
    
    # Disable command
    subparsers.add_parser('disable', help='Disable fault injection')
    
    # Status command
    subparsers.add_parser('status', help='Show fault injection status and statistics')
    
    # Inject command
    inject_parser = subparsers.add_parser('inject', help='Inject fault into a frame')
    inject_parser.add_argument('frame', help='Modbus frame in hex')
    inject_parser.add_argument('--function-code', '-f', type=int, default=0x03,
                               help='Modbus function code (default: 3)')
    
    # Reset command
    subparsers.add_parser('reset', help='Reset statistics')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    try:
        if args.command == 'test':
            logging.basicConfig(level=logging.DEBUG)
            test_fault_injector()
            return 0
            
        elif args.command == 'enable':
            result = enable_fault_injection(args.fault_type, args.probability, args.duration)
            if result['success']:
                print(f"✓ Fault injection enabled:")
                print(f"  Type: {result['config']['fault_type']}")
                print(f"  Probability: {result['config']['probability']}%")
                print(f"  Duration: {result['config']['duration_seconds']}s")
                return 0
            else:
                print(f"✗ Failed: {result.get('error', 'Unknown error')}")
                return 1
                
        elif args.command == 'disable':
            result = disable_fault_injection()
            if result['success']:
                stats = result['statistics']
                print(f"✓ Fault injection disabled")
                print(f"  Faults injected: {stats['faults_injected']}")
                print(f"  Requests processed: {stats['requests_processed']}")
                return 0
            else:
                print(f"✗ Failed to disable")
                return 1
                
        elif args.command == 'status':
            status = get_fault_status()
            print("\n=== Fault Injection Status ===")
            print(f"Enabled: {status['enabled']}")
            if status['enabled']:
                print(f"Type: {status['fault_type']}")
                print(f"Probability: {status['probability']}%")
                print(f"Duration: {status['duration_seconds']}s")
                if status.get('time_remaining'):
                    print(f"Time remaining: {status['time_remaining']:.1f}s")
            print(f"\nStatistics:")
            print(f"  Faults injected: {status['faults_injected']}")
            print(f"  Requests processed: {status['requests_processed']}")
            print(f"\nDetailed Stats:")
            for key, value in status['detailed_stats'].items():
                print(f"  {key}: {value}")
            print()
            return 0
            
        elif args.command == 'inject':
            print(f"\n=== Injecting Fault ===")
            print(f"Original frame: {args.frame}")
            modified = inject_fault(args.frame, args.function_code)
            if modified != args.frame:
                print(f"Modified frame: {modified}")
            else:
                print(f"No fault injected (fault disabled or probability miss)")
            print()
            return 0
            
        elif args.command == 'reset':
            result = reset_statistics()
            if result['success']:
                print(f"✓ Statistics reset")
                return 0
            else:
                print(f"✗ Failed to reset statistics")
                return 1
            
    except Exception as e:
        print(f"Error: {e}")
        logger.exception("Command failed")
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
