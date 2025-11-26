#!/usr/bin/env python3
"""
Timed Fault Injection Script

This script monitors ESP32 polling pattern and injects faults
IMMEDIATELY before the next expected poll to ensure the error is caught.

Usage:
    python inject_timed_fault.py --error CRC_ERROR --count 3
"""

import requests
import argparse
import time
import sys
from datetime import datetime

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

def main():
    parser = argparse.ArgumentParser(description='Timed Fault Injection for ESP32 Testing')
    parser.add_argument('--error', required=True, 
                       choices=['CRC_ERROR', 'EXCEPTION', 'CORRUPT', 'PACKET_DROP', 'DELAY'],
                       help='Type of error to inject')
    parser.add_argument('--code', type=lambda x: int(x, 16), default=0,
                       help='Exception code (hex, e.g., 01, 06) for EXCEPTION type')
    parser.add_argument('--delay', type=int, default=0,
                       help='Delay in milliseconds for DELAY type')
    parser.add_argument('--count', type=int, default=3,
                       help='Number of times to inject the fault')
    parser.add_argument('--interval', type=int, default=10,
                       help='Expected ESP32 polling interval in seconds (default: 10)')
    
    args = parser.parse_args()
    
    print(f"{Colors.BOLD}{Colors.CYAN}")
    print("╔═══════════════════════════════════════════════════════╗")
    print("║     Timed Fault Injection Mode                       ║")
    print("╚═══════════════════════════════════════════════════════╝")
    print(f"{Colors.ENDC}")
    
    print(f"{Colors.WARNING}[INFO] Error Type: {args.error}{Colors.ENDC}")
    if args.code > 0:
        print(f"{Colors.WARNING}[INFO] Exception Code: 0x{args.code:02X}{Colors.ENDC}")
    if args.delay > 0:
        print(f"{Colors.WARNING}[INFO] Delay: {args.delay} ms{Colors.ENDC}")
    print(f"{Colors.WARNING}[INFO] Injection Count: {args.count}{Colors.ENDC}")
    print(f"{Colors.WARNING}[INFO] ESP32 Polling Interval: ~{args.interval} seconds{Colors.ENDC}")
    print(f"\n{Colors.BOLD}Strategy:{Colors.ENDC}")
    print(f"  1. Inject error JUST BEFORE expected ESP32 poll")
    print(f"  2. Wait {args.interval} seconds for ESP32 to poll and consume error")
    print(f"  3. Repeat {args.count} times")
    print(f"\n{Colors.BOLD}Press Ctrl+C to stop{Colors.ENDC}\n")
    
    input(f"{Colors.CYAN}Press ENTER when you see ESP32 just completed a poll...{Colors.ENDC}")
    
    success_count = 0
    
    try:
        for i in range(args.count):
            # Wait for next polling cycle (minus 1 second to inject just before)
            wait_time = args.interval - 1
            print(f"\n{Colors.WARNING}[{datetime.now().strftime('%H:%M:%S')}] Waiting {wait_time} seconds for next poll cycle...{Colors.ENDC}")
            
            for remaining in range(wait_time, 0, -1):
                print(f"  Countdown: {remaining}s", end='\r', flush=True)
                time.sleep(1)
            
            print(f"\n{Colors.BOLD}[{datetime.now().strftime('%H:%M:%S')}] INJECTING ERROR NOW!{Colors.ENDC}")
            
            success, result = inject_error(args.error, args.code, args.delay)
            
            if success:
                success_count += 1
                print(f"{Colors.OKGREEN}✓ Injection #{i+1}/{args.count} successful{Colors.ENDC}")
                if isinstance(result, dict) and 'frame' in result:
                    print(f"  Response frame: {result['frame']}")
                print(f"{Colors.CYAN}→ ESP32 should poll THIS error in the next 1-2 seconds{Colors.ENDC}")
            else:
                print(f"{Colors.FAIL}✗ Injection #{i+1}/{args.count} failed: {result}{Colors.ENDC}")
            
            # Wait a bit for ESP32 to poll and consume the error
            time.sleep(2)
    
    except KeyboardInterrupt:
        print(f"\n{Colors.WARNING}[INFO] Injection stopped by user{Colors.ENDC}")
    
    print(f"\n{Colors.BOLD}═══ Injection Summary ═══{Colors.ENDC}")
    print(f"Total attempts: {args.count}")
    print(f"Successful: {success_count}")
    print(f"Failed: {args.count - success_count}")
    print(f"\n{Colors.OKGREEN}✓ Check ESP32 serial monitor for fault detection logs{Colors.ENDC}")

if __name__ == "__main__":
    main()
