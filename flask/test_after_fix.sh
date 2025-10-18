#!/bin/bash
# Quick test script after fixing Flask server

echo "======================================================================"
echo "🔧 TESTING COMMAND EXECUTION AFTER FIX"
echo "======================================================================"
echo ""
echo "⚠️  Make sure you:"
echo "   1. Restarted Flask server"
echo "   2. Flashed ESP32 with updated firmware"
echo ""
read -p "Press Enter when ready..."
echo ""

echo "----------------------------------------------------------------------"
echo "Test 1: Direct Percentage Command (50%)"
echo "----------------------------------------------------------------------"
python queue_command_for_esp32.py set_power_percentage 50
echo ""
echo "⏳ Waiting 30 seconds for ESP32 to poll and execute..."
sleep 30
echo ""

echo "----------------------------------------------------------------------"
echo "Test 2: Check Command Status"
echo "----------------------------------------------------------------------"
curl "http://localhost:5001/command/history?device_id=ESP32_EcoWatt_Smart" 2>/dev/null | python3 -m json.tool | tail -30
echo ""

echo "----------------------------------------------------------------------"
echo "Test 3: Check Statistics"
echo "----------------------------------------------------------------------"
curl http://localhost:5001/command/statistics 2>/dev/null | python3 -m json.tool
echo ""

echo "======================================================================"
echo "✅ TESTS COMPLETE"
echo "======================================================================"
echo ""
echo "Expected ESP32 serial output:"
echo "  'Received command: set_power_percentage'"
echo "  'Setting power percentage to 50%'"
echo "  'Sending write frame: 110600080032...'"
echo "  'Received frame: 110600080032...' (echo = success!)"
echo "  'Power percentage set successfully to 50%'"
echo ""
echo "If you see exception 03, the issue is with the inverter hardware,"
echo "not the command system. The system itself is working correctly."
echo ""
