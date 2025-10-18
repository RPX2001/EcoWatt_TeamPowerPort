#!/usr/bin/env python3
"""
Queue Command for Real ESP32 Device
=====================================
This script queues a command that will be picked up by your real ESP32
during its next upload cycle.

Usage:
    python queue_command_for_esp32.py set_power 5000              # Power in watts (converted to %)
    python queue_command_for_esp32.py set_power_percentage 50     # Direct percentage (0-100)
    python queue_command_for_esp32.py write_register 40001 4500
"""

import requests
import json
import sys
import time

SERVER_URL = "http://localhost:5001"
DEVICE_ID = "ESP32_EcoWatt_Smart"

def queue_set_power(power_value):
    """Queue a set_power command (will be converted to percentage by ESP32)"""
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "set_power",
        "parameters": {
            "power_value": int(power_value)
        }
    }
    
    # Calculate what percentage this will be (assuming 10kW max capacity)
    percentage = min(100, (int(power_value) * 100) // 10000)
    
    print("\n" + "="*60)
    print("QUEUING COMMAND FOR REAL ESP32")
    print("="*60)
    print(f"Command Type: set_power")
    print(f"Power Value: {power_value} W -> {percentage}% of max capacity")
    print(f"Device: {DEVICE_ID}")
    print("="*60)
    print("\nNOTE: Register 8 accepts PERCENTAGE (0-100%)")
    print(f"   Your {power_value}W will be converted to {percentage}%")
    
    response = requests.post(f"{SERVER_URL}/command/queue", json=payload)
    
    if response.status_code == 200:
        data = response.json()
        command_id = data.get('command_id')
        
        print("\n[SUCCESS] COMMAND QUEUED!")
        print(f"Command ID: {command_id}")
        print("\nNow wait for ESP32 to poll...")
        print("   Your ESP32 will receive this command on the next upload cycle")
        print("\nMonitor ESP32 serial output for:")
        print("   - 'Checking for queued commands from server...'")
        print("   - 'Received command: set_power'")
        print("   - 'Executing command: set_power'")
        print("   - 'Sending command result to server...'")
        
        print("\nTo check command status, run:")
        print(f"   curl http://localhost:5001/command/status/{command_id}")
        
        return command_id
    else:
        print(f"\n[ERROR] {response.status_code}")
        print(response.text)
        return None


def queue_set_power_percentage(percentage):
    """Queue a set_power_percentage command (direct percentage control)"""
    pct = int(percentage)
    if pct < 0: pct = 0
    if pct > 100: pct = 100
    
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "set_power_percentage",
        "parameters": {
            "percentage": pct
        }
    }
    
    print("\n" + "="*60)
    print("QUEUING COMMAND FOR REAL ESP32")
    print("="*60)
    print(f"Command Type: set_power_percentage")
    print(f"Percentage: {pct}%")
    print(f"Device: {DEVICE_ID}")
    print("="*60)
    print(f"\n[INFO] This will directly set Register 8 to {pct}%")
    
    response = requests.post(f"{SERVER_URL}/command/queue", json=payload)
    
    if response.status_code == 200:
        data = response.json()
        command_id = data.get('command_id')
        
        print("\n[SUCCESS] COMMAND QUEUED!")
        print(f"Command ID: {command_id}")
        print("\nNow wait for ESP32 to poll...")
        
        print("\nTo check command status, run:")
        print(f"   curl http://localhost:5001/command/status/{command_id}")
        
        return command_id
    else:
        print(f"\n[ERROR] {response.status_code}")
        print(response.text)
        return None

def queue_write_register(register_address, value):
    """Queue a write_register command"""
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "write_register",
        "parameters": {
            "register_address": int(register_address),
            "value": int(value)
        }
    }
    
    print("\n" + "="*60)
    print("QUEUING COMMAND FOR REAL ESP32")
    print("="*60)
    print(f"Command Type: write_register")
    print(f"Register: {register_address}")
    print(f"Value: {value}")
    print(f"Device: {DEVICE_ID}")
    print("="*60)
    
    response = requests.post(f"{SERVER_URL}/command/queue", json=payload)
    
    if response.status_code == 200:
        data = response.json()
        command_id = data.get('command_id')
        
        print("\n[SUCCESS] COMMAND QUEUED!")
        print(f"Command ID: {command_id}")
        print("\nNow wait for ESP32 to poll...")
        print("   Your ESP32 will receive this command on the next upload cycle")
        
        print("\nTo check command status, run:")
        print(f"   curl http://localhost:5001/command/status/{command_id}")
        
        return command_id
    else:
        print(f"\n[ERROR] {response.status_code}")
        print(response.text)
        return None

def monitor_command(command_id):
    """Monitor command status until completed"""
    print("\n" + "="*60)
    print("MONITORING COMMAND STATUS")
    print("="*60)
    print("Press Ctrl+C to stop monitoring\n")
    
    try:
        while True:
            response = requests.get(f"{SERVER_URL}/command/status/{command_id}")
            
            if response.status_code == 200:
                data = response.json()
                status = data.get('status')
                
                print(f"[{time.strftime('%H:%M:%S')}] Status: {status.upper()}", end='')
                
                if status == 'completed':
                    result = data.get('result', 'No result')
                    print(f" [SUCCESS]")
                    print(f"\n[SUCCESS] COMMAND COMPLETED!")
                    print(f"Result: {result}")
                    print(f"Completed at: {data.get('completed_time')}")
                    break
                elif status == 'failed':
                    error = data.get('error', 'Unknown error')
                    print(f" [FAILED]")
                    print(f"\n[ERROR] COMMAND FAILED!")
                    print(f"Error: {error}")
                    break
                elif status == 'sent':
                    print(f" [SENT] (ESP32 received, executing...)")
                else:
                    print(f" [WAITING] (waiting for ESP32 to poll...)")
                
            else:
                print(f"\n[ERROR] Error checking status: {response.status_code}")
                break
            
            time.sleep(5)  # Check every 5 seconds
            
    except KeyboardInterrupt:
        print("\n\n[STOPPED] Monitoring stopped")
        print(f"Check status manually: curl http://localhost:5001/command/status/{command_id}")

def queue_get_peripheral_stats():
    """Queue a get_peripheral_stats command"""
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "get_peripheral_stats",
        "parameters": {}
    }
    
    print("\n" + "="*60)
    print("QUEUING COMMAND FOR REAL ESP32")
    print("="*60)
    print(f"Command Type: get_peripheral_stats")
    print(f"Device: {DEVICE_ID}")
    print("="*60)
    print("\n[INFO] This will retrieve peripheral power gating statistics")
    
    response = requests.post(f"{SERVER_URL}/command/queue", json=payload)
    
    if response.status_code == 200:
        data = response.json()
        command_id = data.get('command_id')
        
        print("\n[SUCCESS] COMMAND QUEUED!")
        print(f"Command ID: {command_id}")
        print("\nWaiting for ESP32 to respond...")
        print("   Statistics will be printed to ESP32 serial monitor")
        
        return command_id
    else:
        print(f"\n[ERROR] {response.status_code}")
        print(response.text)
        return None


def queue_reset_peripheral_stats():
    """Queue a reset_peripheral_stats command"""
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "reset_peripheral_stats",
        "parameters": {}
    }
    
    print("\n" + "="*60)
    print("QUEUING COMMAND FOR REAL ESP32")
    print("="*60)
    print(f"Command Type: reset_peripheral_stats")
    print(f"Device: {DEVICE_ID}")
    print("="*60)
    print("\n[INFO] This will reset peripheral power gating statistics")
    
    response = requests.post(f"{SERVER_URL}/command/queue", json=payload)
    
    if response.status_code == 200:
        data = response.json()
        command_id = data.get('command_id')
        
        print("\n[SUCCESS] COMMAND QUEUED SUCCESSFULLY!")
        print(f"Command ID: {command_id}")
        
        return command_id
    else:
        print(f"\n[ERROR] ERROR: {response.status_code}")
        print(response.text)
        return None


def queue_get_power_stats():
    """Queue a get_power_stats command"""
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "get_power_stats",
        "parameters": {}
    }
    
    print("\n" + "="*60)
    print(" QUEUING COMMAND FOR REAL ESP32")
    print("="*60)
    print(f"Command Type: get_power_stats")
    print(f"Device: {DEVICE_ID}")
    print("="*60)
    print("\n� This will retrieve power management statistics")
    
    response = requests.post(f"{SERVER_URL}/command/queue", json=payload)
    
    if response.status_code == 200:
        data = response.json()
        command_id = data.get('command_id')
        
        print("\n[SUCCESS] COMMAND QUEUED SUCCESSFULLY!")
        print(f"Command ID: {command_id}")
        
        return command_id
    else:
        print(f"\n[ERROR] ERROR: {response.status_code}")
        print(response.text)
        return None


def show_usage():
    """Show usage instructions"""
    print("\n" + "="*60)
    print("�📋 QUEUE COMMAND FOR ESP32 - USAGE")
    print("="*60)
    print("\nExamples:")
    print("  python queue_command_for_esp32.py set_power 5000")
    print("  python queue_command_for_esp32.py set_power_percentage 50")
    print("  python queue_command_for_esp32.py write_register 40001 4500")
    print("  python queue_command_for_esp32.py get_peripheral_stats")
    print("  python queue_command_for_esp32.py get_power_stats")
    print("\n[WARNING]  IMPORTANT: Register 8 accepts PERCENTAGE (0-100%), not watts!")
    print("\nCommands:")
    print("  set_power <watts>")
    print("    - Sets power (automatically converted to %)")
    print("    - watts: 0-10000W (converted to 0-100%)")
    print("")
    print("  set_power_percentage <percentage>")
    print("    - Direct percentage control")
    print("    - percentage: 0-100%")
    print("")
    print("  write_register <address> <value>")
    print("    - Writes value to Modbus register")
    print("    - address: Register address (e.g., 40001)")
    print("    - value: Value to write (e.g., 4500)")
    print("")
    print("  get_peripheral_stats")
    print("    - Get peripheral power gating statistics")
    print("    - Shows UART duty cycle and power savings")
    print("")
    print("  reset_peripheral_stats")
    print("    - Reset peripheral power gating statistics")
    print("")
    print("  get_power_stats")
    print("    - Get power management statistics")
    print("\nOptions:")
    print("  --monitor : Monitor command until completion")
    print("="*60 + "\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        show_usage()
        sys.exit(1)
    
    command_type = sys.argv[1]
    monitor = "--monitor" in sys.argv
    
    command_id = None
    
    if command_type == "set_power":
        if len(sys.argv) < 3:
            print("[ERROR] Error: Missing power value")
            print("Usage: python queue_command_for_esp32.py set_power <value>")
            sys.exit(1)
        
        power_value = sys.argv[2]
        command_id = queue_set_power(power_value)
        
    elif command_type == "set_power_percentage":
        if len(sys.argv) < 3:
            print("[ERROR] Error: Missing percentage value")
            print("Usage: python queue_command_for_esp32.py set_power_percentage <percentage>")
            sys.exit(1)
        
        percentage = sys.argv[2]
        command_id = queue_set_power_percentage(percentage)
        
    elif command_type == "write_register":
        if len(sys.argv) < 4:
            print("[ERROR] Error: Missing register address or value")
            print("Usage: python queue_command_for_esp32.py write_register <address> <value>")
            sys.exit(1)
        
        register_address = sys.argv[2]
        value = sys.argv[3]
        command_id = queue_write_register(register_address, value)
    
    elif command_type == "get_peripheral_stats":
        command_id = queue_get_peripheral_stats()
    
    elif command_type == "reset_peripheral_stats":
        command_id = queue_reset_peripheral_stats()
    
    elif command_type == "get_power_stats":
        command_id = queue_get_power_stats()
        
    else:
        print(f"[ERROR] Error: Unknown command type '{command_type}'")
        show_usage()
        sys.exit(1)
    
    # Monitor if requested
    if command_id and monitor:
        monitor_command(command_id)
    elif command_id:
        print("\n[TIP] TIP: Add --monitor to automatically track command status")
        print(f"   Example: python queue_command_for_esp32.py set_power 5000 --monitor\n")
