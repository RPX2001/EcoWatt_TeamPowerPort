"""
Test Script for Command Queue System
Allows queuing commands via Flask API for ESP32 to execute
"""

import requests
import json
import time

# Configuration
FLASK_SERVER = "http://10.78.228.2:5001"
DEVICE_ID = "ESP32_EcoWatt_Smart"

def queue_command(command_type, parameters):
    """Queue a command for the ESP32 to execute."""
    url = f"{FLASK_SERVER}/command/queue"
    
    payload = {
        "device_id": DEVICE_ID,
        "command_type": command_type,
        "parameters": parameters
    }
    
    try:
        response = requests.post(url, json=payload, timeout=5)
        print(f"\n{'='*60}")
        print(f"Queue Command Response (Status: {response.status_code})")
        print(f"{'='*60}")
        print(json.dumps(response.json(), indent=2))
        
        if response.status_code == 200:
            return response.json().get('command_id')
        else:
            return None
    except Exception as e:
        print(f"Error queuing command: {e}")
        return None

def get_command_status(command_id):
    """Get the status of a specific command."""
    url = f"{FLASK_SERVER}/command/status"
    params = {"command_id": command_id}
    
    try:
        response = requests.get(url, params=params, timeout=5)
        print(f"\n{'='*60}")
        print(f"Command Status Response (Status: {response.status_code})")
        print(f"{'='*60}")
        print(json.dumps(response.json(), indent=2))
    except Exception as e:
        print(f"Error getting command status: {e}")

def get_command_history():
    """Get command history for the device."""
    url = f"{FLASK_SERVER}/command/history"
    params = {"device_id": DEVICE_ID}
    
    try:
        response = requests.get(url, params=params, timeout=5)
        print(f"\n{'='*60}")
        print(f"Command History (Status: {response.status_code})")
        print(f"{'='*60}")
        data = response.json()
        if 'commands' in data:
            for cmd in data['commands']:
                print(f"  {cmd['command_id']}: {cmd['command_type']} - Status: {cmd['status']}")
                if cmd.get('result'):
                    print(f"    Result: {cmd['result']}")
        else:
            print(json.dumps(data, indent=2))
    except Exception as e:
        print(f"Error getting command history: {e}")

def main():
    print(f"\n{'='*60}")
    print(f"Command Queue Test Script")
    print(f"Target: {DEVICE_ID} @ {FLASK_SERVER}")
    print(f"{'='*60}")
    
    # Test 1: Queue a set_power command
    print("\n[TEST 1] Queuing set_power command (50W)...")
    cmd_id1 = queue_command("set_power", {"power_value": 50})
    
    # Test 2: Queue another set_power command
    print("\n[TEST 2] Queuing set_power command (100W)...")
    cmd_id2 = queue_command("set_power", {"power_value": 100})
    
    # Test 3: Queue a write_register command
    print("\n[TEST 3] Queuing write_register command...")
    cmd_id3 = queue_command("write_register", {
        "register_address": 8,
        "value": 75
    })
    
    # Wait a bit
    print("\n[INFO] Waiting 2 seconds...")
    time.sleep(2)
    
    # Check status of first command
    if cmd_id1:
        print(f"\n[TEST 4] Checking status of command {cmd_id1}...")
        get_command_status(cmd_id1)
    
    # Get full history
    print("\n[TEST 5] Getting full command history...")
    get_command_history()
    
    print(f"\n{'='*60}")
    print("Test completed. Commands are queued and waiting for ESP32 to retrieve them.")
    print("ESP32 will check for commands during its next upload cycle.")
    print(f"{'='*60}\n")

if __name__ == "__main__":
    main()
