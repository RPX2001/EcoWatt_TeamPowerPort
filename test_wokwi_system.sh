#!/bin/bash
# EcoWatt Complete System Test with Wokwi
# Tests Flask server + ESP32 OTA with fault injection

set -e  # Exit on error

PROJECT_ROOT="/home/akitha/Desktop/EcoWatt_TeamPowerPort"
FLASK_DIR="$PROJECT_ROOT/flask"
ESP32_DIR="$PROJECT_ROOT/PIO/ECOWATT"

echo "======================================================"
echo "EcoWatt System Test - Wokwi + Flask + FOTA"
echo "======================================================"
echo ""

# Step 1: Build ESP32 Firmware
echo "📦 Step 1: Building ESP32 firmware for Wokwi..."
cd "$ESP32_DIR"
platformio run -e wokwi

if [ $? -eq 0 ]; then
    echo "✅ ESP32 firmware built successfully!"
else
    echo "❌ ESP32 build failed!"
    exit 1
fi

echo ""
echo "======================================================"
echo "✅ Build Complete!"
echo "======================================================"
echo ""
echo "Next steps:"
echo "1. Start Flask server in Terminal 2:"
echo "   cd $FLASK_DIR"
echo "   python3 flask_server_modular.py"
echo ""
echo "2. Start Wokwi simulation in Terminal 3:"
echo "   cd $ESP32_DIR"
echo "   platformio run -e wokwi -t wokwi"
echo ""
echo "3. Test fault injection in Terminal 4:"
echo "   cd $FLASK_DIR"
echo "   curl -X POST http://localhost:5001/ota/test/enable \\"
echo "     -H 'Content-Type: application/json' \\"
echo "     -d '{\"fault_type\": \"corrupt_chunk\"}'"
echo ""
echo "======================================================"
