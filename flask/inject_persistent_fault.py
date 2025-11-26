#!/usr/bin/env python3
"""
Persistent Fault Injection Script for EcoWatt Testing

This script continuously injects faults to ensure the ESP32 encounters them.
The Error Emulation API is one-shot (clears after one request), so this script
keeps re-injecting the error in a loop to ensure the ESP32 catches it.

Usage:
    python inject_persistent_fault.py --error CRC_ERROR --duration 30
    python inject_persistent_fault.py --error EXCEPTION --code 06 --duration 60
    
    Press Ctrl+C to stop injection
"""

import requests
import argparse
import time
import sys
from datetime import datetime

# API Endpoint
ERROR_API = "http://20.15.114.131:8080/api/inverter/error"

class Colors:
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    CYAN = '\033[96m'

def inject_error(error_type: str, exception_code: int = 0, delay_ms: int = 0):
    """Inject a single fault"""
    payload = {
        "slaveAddress": 17,
        "functionCode": 4,
        "errorType": error_type,
        "exceptionCode": exception_code,
        "delayMs": delay_ms
    }
    
    try:
        response = requests.post(ERROR_API, json=payload, timeout=2)
        if response.status_code == 200:
            return True, response.json()
        else:
            return False, f"HTTP {response.status_code}"
    except Exception as e:
        return False, str(e)

def continuous_injection(error_type: str, exception_code: int, delay_ms: int, duration: int):
    """Continuously inject faults for a specified duration"""
    
    print(f"{Colors.BOLD}{Colors.CYAN}")
    print("╔═══════════════════════════════════════════════════════╗")
    print("║     Continuous Fault Injection Mode                  ║")
    print("╚═══════════════════════════════════════════════════════╝")
    print(f"{Colors.ENDC}")
    
    print(f"{Colors.WARNING}[INFO] Error Type: {error_type}{Colors.ENDC}")
    if exception_code > 0:
        print(f"{Colors.WARNING}[INFO] Exception Code: 0x{exception_code:02X}{Colors.ENDC}")
    if delay_ms > 0:
        print(f"{Colors.WARNING}[INFO] Delay: {delay_ms} ms{Colors.ENDC}")
    print(f"{Colors.WARNING}[INFO] Duration: {duration} seconds{Colors.ENDC}")
    print(f"{Colors.WARNING}[INFO] Injection Interval: Every 3 seconds{Colors.ENDC}")
    print(f"\n{Colors.BOLD}Press Ctrl+C to stop{Colors.ENDC}\n")
    
    injection_count = 0
    success_count = 0
    start_time = time.time()
    
    try:
        while (time.time() - start_time) < duration:
            injection_count += 1
            timestamp = datetime.now().strftime("%H:%M:%S")
            
            success, result = inject_error(error_type, exception_code, delay_ms)
            
            if success:
                success_count += 1
                print(f"{Colors.OKGREEN}[{timestamp}] ✓ Injection #{injection_count} successful{Colors.ENDC}")
                if isinstance(result, dict) and 'frame' in result:
                    print(f"           Response frame: {result['frame']}")
            else:
                print(f"{Colors.FAIL}[{timestamp}] ✗ Injection #{injection_count} failed: {result}{Colors.ENDC}")
            
            # Wait 3 seconds before next injection
            # (ESP32 polls every ~10 seconds, so this ensures multiple chances to catch the error)
            time.sleep(3)
    
    except KeyboardInterrupt:
        print(f"\n{Colors.WARNING}[INFO] Injection stopped by user{Colors.ENDC}")
    
    # Summary
    elapsed = time.time() - start_time
    print(f"\n{Colors.BOLD}═══ Injection Summary ═══{Colors.ENDC}")
    print(f"Total injections: {injection_count}")
    print(f"Successful: {success_count}")
    print(f"Failed: {injection_count - success_count}")
    print(f"Duration: {elapsed:.1f} seconds")
    print(f"\n{Colors.OKGREEN}✓ Check ESP32 serial monitor for fault detection logs{Colors.ENDC}\n")

def main():
    parser = argparse.ArgumentParser(
        description='Continuously inject faults for ESP32 testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python inject_persistent_fault.py --error CRC_ERROR --duration 30
  python inject_persistent_fault.py --error EXCEPTION --code 06 --duration 60
  python inject_persistent_fault.py --error DELAY --delay 2000 --duration 45
  python inject_persistent_fault.py --error CORRUPT --duration 30
        '''
    )
    
    parser.add_argument('--error', required=True, 
                       choices=['EXCEPTION', 'CRC_ERROR', 'CORRUPT', 'PACKET_DROP', 'DELAY'],
                       help='Type of error to inject')
    parser.add_argument('--code', type=lambda x: int(x, 16), default=0,
                       help='Modbus exception code (hex, e.g., 06 for Busy)')
    parser.add_argument('--delay', type=int, default=0,
                       help='Delay in milliseconds (for DELAY type)')
    parser.add_argument('--duration', type=int, default=30,
                       help='Duration of injection in seconds (default: 30)')
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.error == 'EXCEPTION' and args.code == 0:
        print(f"{Colors.FAIL}[ERROR] EXCEPTION type requires --code argument{Colors.ENDC}")
        print(f"Example: --error EXCEPTION --code 06")
        sys.exit(1)
    
    if args.error == 'DELAY' and args.delay == 0:
        print(f"{Colors.FAIL}[ERROR] DELAY type requires --delay argument{Colors.ENDC}")
        print(f"Example: --error DELAY --delay 2000")
        sys.exit(1)
    
    # Run continuous injection
    continuous_injection(args.error, args.code, args.delay, args.duration)

if __name__ == '__main__':
    main()
