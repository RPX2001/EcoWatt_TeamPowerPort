#!/bin/bash
# Simple script to watch for ESP32 serial output
# This monitors Flask server logs for ESP32 connections as an alternative

echo "Monitoring Flask server for ESP32 activity..."
echo "Press Ctrl+C to stop"
echo "=========================================="
echo ""

cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask

# Watch for ESP32 requests in real-time
tail -f /dev/null 2>&1 &
TAIL_PID=$!

# Poll Flask server for activity
while true; do
    # Check if ESP32 made any requests
    curl -s http://127.0.0.1:5001/stats 2>/dev/null | grep -E "(device_id|total_requests|ESP32)" && echo ""
    sleep 5
done

kill $TAIL_PID 2>/dev/null
