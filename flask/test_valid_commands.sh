#!/bin/bash
# Test different power values to find what the inverter accepts

echo "======================================================================"
echo "🧪 TESTING DIFFERENT POWER VALUES"
echo "======================================================================"
echo ""
echo "This will test different power values to find what your inverter accepts"
echo ""

# Test various power values
POWER_VALUES=(1000 2000 3000 4000 6000)

for power in "${POWER_VALUES[@]}"; do
    echo "----------------------------------------------------------------------"
    echo "Testing power value: ${power}W"
    echo "----------------------------------------------------------------------"
    
    python queue_command_for_esp32.py set_power "$power"
    
    echo ""
    echo "⏳ Waiting 45 seconds for ESP32 to execute..."
    sleep 45
    
    echo ""
done

echo "======================================================================"
echo "✅ ALL TESTS QUEUED"
echo "======================================================================"
echo ""
echo "Check statistics:"
echo "  curl http://localhost:5001/command/statistics | jq"
echo ""
echo "Check history:"
echo '  curl "http://localhost:5001/command/history?device_id=ESP32_EcoWatt_Smart" | jq'
echo ""
