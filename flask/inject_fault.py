#!/usr/bin/env python3
"""
Fault Injection Script for EcoWatt Fault Recovery Testing

This script injects various types of faults into the inverter simulator
to test the ESP32 fault detection and recovery mechanisms.

Usage:
    python inject_fault.py --error EXCEPTION --code 06
    python inject_fault.py --error TIMEOUT
    python inject_fault.py --error CRC_ERROR
    python inject_fault.py --clear

Error Types:
    EXCEPTION     - Modbus exception with specific code (01-0B)
    CRC_ERROR     - CRC validation error
    CORRUPT       - Malformed/corrupt frame
    PACKET_DROP   - Simulates packet loss (timeout)
    DELAY         - Slow response simulation

Modbus Exception Codes:
    01 - Illegal Function
    02 - Illegal Data Address
    03 - Illegal Data Value
    04 - Slave Device Failure
    05 - Acknowledge
    06 - Slave Device Busy
    08 - Memory Parity Error
    0A - Gateway Path Unavailable
    0B - Gateway Target Device Failed to Respond
"""

import requests
import argparse
import sys
import json
from typing import Optional

# API Endpoints
ERROR_EMULATION_API = "http://20.15.114.131:8080/api/inverter/error"
ADD_ERROR_FLAG_API = "http://20.15.114.131:8080/api/user/error-flag/add"

# Authentication (if needed for production API)
API_TOKEN = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ=="

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

def print_banner():
    """Print script banner"""
    print(f"{Colors.HEADER}{Colors.BOLD}")
    print("╔═══════════════════════════════════════════════════════╗")
    print("║     EcoWatt Fault Injection Testing Tool             ║")
    print("║     Milestone 5 Part 2: Fault Recovery               ║")
    print("╚═══════════════════════════════════════════════════════╝")
    print(f"{Colors.ENDC}")

def inject_error_emulation(error_type: str, exception_code: Optional[str] = None, delay_ms: Optional[int] = None) -> bool:
    """
    Inject fault using Error Emulation API (direct testing, no auth required)
    
    Args:
        error_type: Type of error (EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY)
        exception_code: Modbus exception code (hex string, e.g., "06")
        delay_ms: Delay in milliseconds for DELAY type
    
    Returns:
        True if injection successful
    """
    try:
        # Build payload according to API specification
        payload = {
            "slaveAddress": 0x11,  # 17 decimal (standard slave address)
            "functionCode": 0x04,  # Read Input Registers
            "errorType": error_type,
            "exceptionCode": 0,
            "delayMs": 0
        }
        
        # Set exception code if EXCEPTION type
        if error_type == "EXCEPTION" and exception_code:
            payload["exceptionCode"] = int(exception_code, 16)  # Convert hex string to int
        
        # Set delay if DELAY type
        if error_type == "DELAY" and delay_ms:
            payload["delayMs"] = delay_ms
        
        print(f"{Colors.OKBLUE}[INFO] Injecting fault via Error Emulation API{Colors.ENDC}")
        print(f"       URL: {ERROR_EMULATION_API}")
        print(f"       Type: {error_type}")
        if exception_code:
            print(f"       Exception Code: 0x{exception_code}")
        if delay_ms:
            print(f"       Delay: {delay_ms} ms")
        print(f"       Payload: {json.dumps(payload, indent=2)}")
        
        response = requests.post(ERROR_EMULATION_API, json=payload, timeout=10)
        
        if response.status_code == 200:
            print(f"{Colors.OKGREEN}[SUCCESS] Fault injected successfully{Colors.ENDC}")
            result = response.json()
            print(f"          Response: {json.dumps(result, indent=2)}")
            if "frame" in result and result["frame"]:
                print(f"          Frame: {result['frame']}")
            return True
        else:
            print(f"{Colors.FAIL}[ERROR] Failed to inject fault{Colors.ENDC}")
            print(f"        Status: {response.status_code}")
            print(f"        Response: {response.text}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"{Colors.FAIL}[ERROR] Request failed: {e}{Colors.ENDC}")
        return False

def inject_error_flag(error_type: str, exception_code: Optional[str] = None, delay_ms: Optional[int] = None) -> bool:
    """
    Inject fault using Add Error Flag API (production-like testing, requires auth)
    
    Args:
        error_type: Type of error (EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY)
        exception_code: Modbus exception code (hex string, e.g., "06")
        delay_ms: Delay in milliseconds for DELAY type
    
    Returns:
        True if injection successful
    """
    try:
        # Build payload according to API specification
        payload = {
            "errorType": error_type,
            "exceptionCode": 0,
            "delayMs": 0
        }
        
        # Set exception code if EXCEPTION type
        if error_type == "EXCEPTION" and exception_code:
            payload["exceptionCode"] = int(exception_code, 16)  # Convert hex string to int
        
        # Set delay if DELAY type
        if error_type == "DELAY" and delay_ms:
            payload["delayMs"] = delay_ms
        
        headers = {
            "Authorization": API_TOKEN,
            "Content-Type": "application/json"
        }
        
        print(f"{Colors.OKBLUE}[INFO] Setting error flag via Add Error Flag API{Colors.ENDC}")
        print(f"       URL: {ADD_ERROR_FLAG_API}")
        print(f"       Type: {error_type}")
        if exception_code:
            print(f"       Exception Code: 0x{exception_code}")
        if delay_ms:
            print(f"       Delay: {delay_ms} ms")
        print(f"       Payload: {json.dumps(payload, indent=2)}")
        
        response = requests.post(ADD_ERROR_FLAG_API, json=payload, headers=headers, timeout=10)
        
        if response.status_code == 200:
            print(f"{Colors.OKGREEN}[SUCCESS] Error flag set successfully{Colors.ENDC}")
            print(f"{Colors.WARNING}[NOTICE] Error will be returned on NEXT ESP32 request{Colors.ENDC}")
            return True
        else:
            print(f"{Colors.FAIL}[ERROR] Failed to set error flag{Colors.ENDC}")
            print(f"        Status: {response.status_code}")
            print(f"        Response: {response.text}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"{Colors.FAIL}[ERROR] Request failed: {e}{Colors.ENDC}")
        return False

def clear_error_flag() -> bool:
    """Clear any active error flags"""
    try:
        # Assuming there's a clear endpoint - adjust if needed
        clear_url = "http://20.15.114.131:8080/api/user/error-flag/clear"
        
        headers = {
            "Authorization": API_TOKEN,
            "Content-Type": "application/json"
        }
        
        print(f"{Colors.OKBLUE}[INFO] Clearing error flags{Colors.ENDC}")
        response = requests.post(clear_url, headers=headers, timeout=10)
        
        if response.status_code == 200:
            print(f"{Colors.OKGREEN}[SUCCESS] Error flags cleared{Colors.ENDC}")
            return True
        else:
            print(f"{Colors.WARNING}[WARNING] Clear endpoint returned: {response.status_code}{Colors.ENDC}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"{Colors.WARNING}[WARNING] Could not clear flags: {e}{Colors.ENDC}")
        return False

def validate_exception_code(code: str) -> bool:
    """Validate Modbus exception code"""
    valid_codes = ["01", "02", "03", "04", "05", "06", "08", "0A", "0B"]
    return code.upper() in valid_codes

def main():
    parser = argparse.ArgumentParser(
        description="Inject faults into inverter simulator for testing ESP32 fault recovery",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Inject Modbus exception (Slave Device Busy)
  python inject_fault.py --error EXCEPTION --code 06
  
  # Inject CRC error
  python inject_fault.py --error CRC_ERROR
  
  # Inject corrupt frame
  python inject_fault.py --error CORRUPT
  
  # Simulate packet drop/timeout
  python inject_fault.py --error PACKET_DROP
  
  # Simulate slow response (2 second delay)
  python inject_fault.py --error DELAY --delay 2000
  
  # Use production API (error flag)
  python inject_fault.py --error EXCEPTION --code 04 --production
  
  # Clear error flags
  python inject_fault.py --clear
        """
    )
    
    parser.add_argument(
        "--error",
        choices=["EXCEPTION", "CRC_ERROR", "CORRUPT", "PACKET_DROP", "DELAY"],
        help="Type of error to inject"
    )
    
    parser.add_argument(
        "--code",
        help="Modbus exception code (hex: 01, 02, 03, 04, 05, 06, 08, 0A, 0B)"
    )
    
    parser.add_argument(
        "--delay",
        type=int,
        help="Delay in milliseconds (for DELAY error type)"
    )
    
    parser.add_argument(
        "--production",
        action="store_true",
        help="Use production API (Add Error Flag) instead of Error Emulation"
    )
    
    parser.add_argument(
        "--clear",
        action="store_true",
        help="Clear any active error flags"
    )
    
    args = parser.parse_args()
    
    # print_banner()
    
    # Handle clear command
    if args.clear:
        return 0 if clear_error_flag() else 1
    
    # Validate arguments
    if not args.error:
        print(f"{Colors.FAIL}[ERROR] --error is required (or use --clear){Colors.ENDC}")
        parser.print_help()
        return 1
    
    if args.error == "EXCEPTION" and not args.code:
        print(f"{Colors.FAIL}[ERROR] --code is required for EXCEPTION error type{Colors.ENDC}")
        return 1
    
    if args.code and not validate_exception_code(args.code):
        print(f"{Colors.FAIL}[ERROR] Invalid exception code: {args.code}{Colors.ENDC}")
        print("Valid codes: 01, 02, 03, 04, 05, 06, 08, 0A, 0B")
        return 1
    
    if args.error == "DELAY" and not args.delay:
        print(f"{Colors.FAIL}[ERROR] --delay is required for DELAY error type{Colors.ENDC}")
        return 1
    
    # Inject the fault
    success = False
    if args.production:
        # Production API (Add Error Flag)
        if args.error == "DELAY":
            print(f"{Colors.WARNING}[WARNING] DELAY type not supported in production API{Colors.ENDC}")
            print("            Use Error Emulation API instead (remove --production flag)")
            return 1
        success = inject_error_flag(args.error, args.code)
    else:
        # Development API (Error Emulation)
        success = inject_error_emulation(args.error, args.code, args.delay)
    
    if success:
        print(f"\n{Colors.OKGREEN}{Colors.BOLD}✓ Fault injection complete{Colors.ENDC}")
        print(f"{Colors.OKCYAN}[NEXT] Monitor ESP32 serial output for fault detection and recovery{Colors.ENDC}")
        return 0
    else:
        print(f"\n{Colors.FAIL}{Colors.BOLD}✗ Fault injection failed{Colors.ENDC}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
