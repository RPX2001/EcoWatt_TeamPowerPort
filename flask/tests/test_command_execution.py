#!/usr/bin/env python3
"""
Test script for Command Execution System
Tests all command endpoints to verify functionality
"""

import requests
import json
import time

# Configuration
BASE_URL = "http://localhost:5001"
DEVICE_ID = "ESP32_EcoWatt_Smart"

def print_response(response, title="Response"):
    """Pretty print HTTP response"""
    print(f"\n{'='*60}")
    print(f"{title}")
    print(f"{'='*60}")
    print(f"Status Code: {response.status_code}")
    try:
        print(f"Response:\n{json.dumps(response.json(), indent=2)}")
    except:
        print(f"Response: {response.text}")
    print(f"{'='*60}\n")

def test_queue_command():
    """Test 1: Queue a set_power command"""
    print("\n🧪 TEST 1: Queue Command (set_power)")
    
    url = f"{BASE_URL}/command/queue"
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "set_power",
        "parameters": {
            "power_value": 5000
        }
    }
    
    response = requests.post(url, json=payload)
    print_response(response, "Test 1: Queue Command")
    
    if response.status_code == 200:
        return response.json().get('command_id')
    return None

def test_poll_command():
    """Test 2: Poll for commands (simulates ESP32)"""
    print("\n🧪 TEST 2: Poll Command")
    
    url = f"{BASE_URL}/command/poll"
    payload = {
        "device_id": DEVICE_ID
    }
    
    response = requests.post(url, json=payload)
    print_response(response, "Test 2: Poll Command")
    
    if response.status_code == 200 and 'command' in response.json():
        return response.json()['command']
    return None

def test_submit_result(command_id):
    """Test 3: Submit command result (simulates ESP32)"""
    print("\n🧪 TEST 3: Submit Command Result")
    
    url = f"{BASE_URL}/command/result"
    payload = {
        "command_id": command_id,
        "status": "completed",
        "result": "Command set_power: executed successfully (TEST)"
    }
    
    response = requests.post(url, json=payload)
    print_response(response, "Test 3: Submit Result")

def test_get_status(command_id):
    """Test 4: Get command status"""
    print("\n🧪 TEST 4: Get Command Status")
    
    url = f"{BASE_URL}/command/status/{command_id}"
    response = requests.get(url)
    print_response(response, "Test 4: Command Status")

def test_get_history():
    """Test 5: Get command history"""
    print("\n🧪 TEST 5: Get Command History")
    
    url = f"{BASE_URL}/command/history?device_id={DEVICE_ID}&limit=10"
    response = requests.get(url)
    print_response(response, "Test 5: Command History")

def test_get_pending():
    """Test 6: Get pending commands"""
    print("\n🧪 TEST 6: Get Pending Commands")
    
    url = f"{BASE_URL}/command/pending?device_id={DEVICE_ID}"
    response = requests.get(url)
    print_response(response, "Test 6: Pending Commands")

def test_get_statistics():
    """Test 7: Get statistics"""
    print("\n🧪 TEST 7: Get Command Statistics")
    
    url = f"{BASE_URL}/command/statistics"
    response = requests.get(url)
    print_response(response, "Test 7: Statistics")

def test_queue_write_register():
    """Test 8: Queue a write_register command"""
    print("\n🧪 TEST 8: Queue write_register Command")
    
    url = f"{BASE_URL}/command/queue"
    payload = {
        "device_id": DEVICE_ID,
        "command_type": "write_register",
        "parameters": {
            "register_address": 40001,
            "value": 4500
        }
    }
    
    response = requests.post(url, json=payload)
    print_response(response, "Test 8: Queue write_register")
    
    if response.status_code == 200:
        return response.json().get('command_id')
    return None

def test_failed_command(command_id):
    """Test 9: Submit failed command result"""
    print("\n🧪 TEST 9: Submit Failed Command Result")
    
    url = f"{BASE_URL}/command/result"
    payload = {
        "command_id": command_id,
        "status": "failed",
        "result": "Inverter SIM timeout - command execution failed (TEST)"
    }
    
    response = requests.post(url, json=payload)
    print_response(response, "Test 9: Failed Command Result")

def test_export_logs():
    """Test 10: Export command logs"""
    print("\n🧪 TEST 10: Export Command Logs")
    
    url = f"{BASE_URL}/command/logs?device_id={DEVICE_ID}"
    response = requests.get(url)
    print_response(response, "Test 10: Export Logs")

def run_all_tests():
    """Run all tests in sequence"""
    print("\n" + "="*60)
    print("COMMAND EXECUTION SYSTEM - COMPREHENSIVE TEST SUITE")
    print("="*60)
    
    try:
        # Test basic workflow
        print("\n📋 PART 1: Basic Command Workflow")
        print("-" * 60)
        
        command_id_1 = test_queue_command()
        if not command_id_1:
            print("❌ Failed to queue command")
            return
        
        time.sleep(0.5)
        
        command = test_poll_command()
        if not command:
            print("❌ Failed to poll command")
            return
        
        time.sleep(0.5)
        
        test_submit_result(command_id_1)
        time.sleep(0.5)
        
        test_get_status(command_id_1)
        
        # Test monitoring endpoints
        print("\n📋 PART 2: Monitoring Endpoints")
        print("-" * 60)
        
        test_get_history()
        time.sleep(0.5)
        
        test_get_pending()
        time.sleep(0.5)
        
        test_get_statistics()
        
        # Test additional commands
        print("\n📋 PART 3: Additional Commands")
        print("-" * 60)
        
        command_id_2 = test_queue_write_register()
        if command_id_2:
            time.sleep(0.5)
            test_failed_command(command_id_2)
        
        time.sleep(0.5)
        test_export_logs()
        
        # Final statistics
        print("\n📋 FINAL STATISTICS")
        print("-" * 60)
        test_get_statistics()
        
        print("\n" + "="*60)
        print("✅ ALL TESTS COMPLETED")
        print("="*60)
        print("\n📝 Check the results above to verify all endpoints work correctly")
        print("📂 Command logs saved to: flask/commands.log")
        print("\n")
        
    except requests.exceptions.ConnectionError:
        print("\n❌ ERROR: Could not connect to Flask server")
        print("Make sure the server is running: python flask/flask_server_hivemq.py")
    except Exception as e:
        print(f"\n❌ ERROR: {e}")

if __name__ == "__main__":
    run_all_tests()
