#!/usr/bin/env python3
"""
Test script to verify fault injection and recovery database functions
"""

from database import Database
import time

print("=" * 60)
print("Testing Fault Injection & Recovery Database")
print("=" * 60)

# Test 1: Save fault injection
print("\n[TEST 1] Saving fault injection...")
injection_id = Database.save_fault_injection(
    device_id=None,
    fault_type="CRC_ERROR",
    backend="inverter_sim",
    error_type="CRC_ERROR",
    exception_code=None,
    delay_ms=None,
    success=True,
    error_msg=None
)
print(f"✅ Saved fault injection with ID: {injection_id}")

# Test 2: Get fault injection history
print("\n[TEST 2] Getting fault injection history...")
history = Database.get_fault_injection_history(limit=10)
print(f"✅ Retrieved {len(history)} injection records")
if history:
    print(f"   Latest: {history[0]}")

# Test 3: Save recovery event
print("\n[TEST 3] Saving recovery event...")
recovery_id = Database.save_recovery_event(
    device_id="ESP32_001",
    timestamp=int(time.time()),
    fault_type="crc_error",
    recovery_action="retry_read",
    success=True,
    details="CRC validation failed, retried successfully after 1 attempt",
    retry_count=1
)
print(f"✅ Saved recovery event with ID: {recovery_id}")

# Test 4: Get recovery events
print("\n[TEST 4] Getting recovery events...")
events = Database.get_recovery_events(device_id="ESP32_001", limit=10)
print(f"✅ Retrieved {len(events)} recovery events")
if events:
    print(f"   Latest: {events[0]}")

# Test 5: Get recovery statistics
print("\n[TEST 5] Getting recovery statistics...")
stats = Database.get_recovery_statistics(device_id="ESP32_001")
print(f"✅ Statistics: {stats}")

# Test 6: Get all recovery events (no device filter)
print("\n[TEST 6] Getting all recovery events...")
all_events = Database.get_recovery_events(device_id=None, limit=10)
print(f"✅ Retrieved {len(all_events)} total events across all devices")

# Test 7: Get global statistics
print("\n[TEST 7] Getting global statistics...")
global_stats = Database.get_recovery_statistics(device_id=None)
print(f"✅ Global statistics: {global_stats}")

print("\n" + "=" * 60)
print("All tests completed successfully!")
print("=" * 60)
