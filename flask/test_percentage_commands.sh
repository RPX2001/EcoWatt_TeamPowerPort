#!/bin/bash
# Test power percentage commands (0-100%)

echo "======================================================================"
echo "🧪 TESTING POWER PERCENTAGE COMMANDS"
echo "======================================================================"
echo ""
echo "Register 8 accepts PERCENTAGE (0-100%), not absolute watts!"
echo ""
echo "Testing different percentages..."
echo ""

# Test various percentages
PERCENTAGES=(0 10 25 50 75 100)

for pct in "${PERCENTAGES[@]}"; do
    echo "----------------------------------------------------------------------"
    echo "Testing: ${pct}% power"
    echo "----------------------------------------------------------------------"
    
    python queue_command_for_esp32.py set_power_percentage "$pct"
    
    echo ""
    echo "⏳ Waiting 45 seconds for ESP32 to execute..."
    sleep 45
    
    echo ""
done

echo "======================================================================"
echo "✅ ALL PERCENTAGE TESTS QUEUED"
echo "======================================================================"
echo ""
echo "Check results:"
echo '  curl "http://localhost:5001/command/history?device_id=ESP32_EcoWatt_Smart" | jq'
echo ""
