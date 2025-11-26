#!/usr/bin/env python3
"""
Continuous Fault Injection Script for EcoWatt Fault Recovery Testing

This script continuously injects faults to ensure the ESP32 encounters them.
The Error Emulation API only affects ONE request, so we inject repeatedly.

Usage:
    python inject_fault_continuous.py --error CRC_ERROR --count 10
    python inject_fault_continuous.py --error EXCEPTION --code 06 --count 5
    python inject_fault_continuous.py --error DELAY --delay 2000 --count 3
"""

import requests
import argparse
import sys
import json
import time
from typing import Optional

# API Endpoints
ERROR_EMULATION_API = "http://20.15.114.131:8080/api/inverter/error"

# Color codes for terminal output
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def inject_fault_once(error_type: str, exception_code: int = 0, delay_ms: int = 0) -> dict:
    """Inject a single fault via Error Emulation API"""
    
    payload = {
        "slaveAddress": 17,
        "functionCode": 4,
        "errorType": error_type,
        "exceptionCode": exception_code,
        "delayMs": delay_ms
    }
    
    try:
        response = requests.post(ERROR_EMULATION_API, json=payload, timeout=5)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"{Colors.FAIL}[ERROR] Failed to inject fault: {e}{Colors.ENDC}")
        return None

def continuous_injection(error_type: str, count: int, exception_code: int = 0, 
                        delay_ms: int = 0, interval: float = 1.0):
    """
    Continuously inject faults at specified interval
    
    Args:
        error_type: Type of error (CRC_ERROR, EXCEPTION, CORRUPT, etc.)
        count: Number of times to inject
        exception_code: Modbus exception code (for EXCEPTION type)
        delay_ms: Response delay in ms (for DELAY type)
        interval: Seconds between injections
    """
    
    print(f"{Colors.HEADER}{'='*70}{Colors.ENDC}")
    print(f"{Colors.HEADER}  CONTINUOUS FAULT INJECTION{Colors.ENDC}")
    print(f"{Colors.HEADER}{'='*70}{Colors.ENDC}")
    print(f"{Colors.OKBLUE}[INFO] Configuration:{Colors.ENDC}")
    print(f"       Error Type: {error_type}")
    print(f"       Injection Count: {count}")
    print(f"       Interval: {interval}s")
    if error_type == "EXCEPTION":
        print(f"       Exception Code: 0x{exception_code:02X}")
    if delay_ms > 0:
        print(f"       Delay: {delay_ms}ms")
    print(f"{Colors.HEADER}{'='*70}{Colors.ENDC}")
    print()
    
    success_count = 0
    fail_count = 0
    
    for i in range(count):
        print(f"{Colors.OKCYAN}[{i+1}/{count}]{Colors.ENDC} Injecting {error_type}...", end=" ", flush=True)
        
        result = inject_fault_once(error_type, exception_code, delay_ms)
        
        if result:
            print(f"{Colors.OKGREEN}✓{Colors.ENDC}")
            success_count += 1
            
            # Show response frame if available
            if "frame" in result:
                print(f"       Response frame: {result['frame']}")
        else:
            print(f"{Colors.FAIL}✗{Colors.ENDC}")
            fail_count += 1
        
        # Wait before next injection (except for last one)
        if i < count - 1:
            time.sleep(interval)
    
    print()
    print(f"{Colors.HEADER}{'='*70}{Colors.ENDC}")
    print(f"{Colors.OKGREEN}[SUMMARY]{Colors.ENDC}")
    print(f"  Success: {success_count}/{count}")
    print(f"  Failed: {fail_count}/{count}")
    print(f"{Colors.HEADER}{'='*70}{Colors.ENDC}")
    print()
    print(f"{Colors.WARNING}[NEXT]{Colors.ENDC} Monitor ESP32 serial output for fault detection and recovery")
    print(f"       Expected to see:")
    print(f"       - '[ERROR] FAULT: ...'")
    print(f"       - 'Recovered: YES/NO'")
    print(f"       - 'Attempt 2: Retrying...' (if recoverable)")
    print()

def main():
    parser = argparse.ArgumentParser(
        description="Continuous Fault Injection Tool for EcoWatt",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Inject 10 CRC errors with 1-second intervals
  python inject_fault_continuous.py --error CRC_ERROR --count 10

  # Inject 5 Modbus exceptions (code 0x06 - Busy) with 2-second intervals
  python inject_fault_continuous.py --error EXCEPTION --code 06 --count 5 --interval 2

  # Inject 3 delayed responses (2 seconds each) with 3-second intervals
  python inject_fault_continuous.py --error DELAY --delay 2000 --count 3 --interval 3

Error Types:
  EXCEPTION   - Modbus exception with specific code (01-0B)
  CRC_ERROR   - CRC validation error
  CORRUPT     - Malformed/corrupt frame
  PACKET_DROP - Simulates packet loss (timeout)
  DELAY       - Slow response simulation

Common Exception Codes:
  01 - Illegal Function (non-recoverable)
  02 - Illegal Data Address (non-recoverable)
  03 - Illegal Data Value (non-recoverable)
  06 - Slave Device Busy (recoverable)
  08 - Memory Parity Error (recoverable)
        """
    )
    
    parser.add_argument(
        '--error',
        type=str,
        required=True,
        choices=['EXCEPTION', 'CRC_ERROR', 'CORRUPT', 'PACKET_DROP', 'DELAY'],
        help='Type of error to inject'
    )
    
    parser.add_argument(
        '--count',
        type=int,
        default=5,
        help='Number of times to inject the error (default: 5)'
    )
    
    parser.add_argument(
        '--interval',
        type=float,
        default=1.0,
        help='Seconds between injections (default: 1.0)'
    )
    
    parser.add_argument(
        '--code',
        type=lambda x: int(x, 16) if x.startswith('0x') else int(x),
        default=0,
        help='Modbus exception code (for EXCEPTION type, e.g., 06 or 0x06)'
    )
    
    parser.add_argument(
        '--delay',
        type=int,
        default=0,
        help='Response delay in milliseconds (for DELAY type)'
    )
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.error == "EXCEPTION" and args.code == 0:
        print(f"{Colors.FAIL}[ERROR] Exception code required for EXCEPTION error type{Colors.ENDC}")
        print(f"       Use --code <hex> (e.g., --code 06 for Slave Busy)")
        sys.exit(1)
    
    if args.error == "DELAY" and args.delay == 0:
        print(f"{Colors.WARNING}[WARNING] DELAY type specified but delay is 0ms{Colors.ENDC}")
        print(f"          Use --delay <ms> (e.g., --delay 2000 for 2-second delay)")
    
    if args.count < 1:
        print(f"{Colors.FAIL}[ERROR] Count must be at least 1{Colors.ENDC}")
        sys.exit(1)
    
    if args.interval < 0:
        print(f"{Colors.FAIL}[ERROR] Interval must be non-negative{Colors.ENDC}")
        sys.exit(1)
    
    # Execute continuous injection
    try:
        continuous_injection(
            error_type=args.error,
            count=args.count,
            exception_code=args.code,
            delay_ms=args.delay,
            interval=args.interval
        )
    except KeyboardInterrupt:
        print(f"\n{Colors.WARNING}[INTERRUPTED] Fault injection stopped by user{Colors.ENDC}")
        sys.exit(0)

if __name__ == "__main__":
    main()
