#!/usr/bin/env python3
"""
Test MQTT Publisher - Simulates Flask server publishing decompressed data

This script publishes test data to demonstrate the mqtt_subscriber.py functionality.
Run this in one terminal and mqtt_subscriber.py in another to see it work.
"""

import paho.mqtt.client as mqtt
import json
import time
import random

BROKER = "broker.hivemq.com"
PORT = 1883
DEVICE_ID = "TEST_ESP32_DEMO"

def publish_test_data():
    """Publish test decompressed data"""
    client_id = f"ecowatt_test_pub_{int(time.time())}"
    client = mqtt.Client(client_id)
    
    print("=" * 80)
    print("🧪 Test MQTT Publisher")
    print("=" * 80)
    print(f"Connecting to {BROKER}:{PORT}...")
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(1)
        
        print(f"✓ Connected! Publishing to: ecowatt/data/{DEVICE_ID}")
        print("=" * 80)
        
        for i in range(5):
            # Create test payload (compressed data format)
            payload = {
                'device_id': DEVICE_ID,
                'timestamp': time.time(),
                'values': [5000 + random.randint(-100, 100) + j for j in range(20)],
                'sample_count': 20,
                'compression_stats': {
                    'method': 'Bit Packing',
                    'ratio': 0.65,
                    'original_size': 40,
                    'compressed_size': 26
                }
            }
            
            topic = f"ecowatt/data/{DEVICE_ID}"
            client.publish(topic, json.dumps(payload))
            print(f"📤 Published message {i+1}/5 with {payload['sample_count']} samples")
            time.sleep(2)
        
        print("\n" + "=" * 80)
        
        # Publish aggregated data format
        payload2 = {
            'device_id': DEVICE_ID,
            'timestamp': time.time(),
            'samples': [
                {'voltage': 5000 + i*10, 'current': 500, 'power': 2000, 'timestamp': int(time.time()*1000)}
                for i in range(15)
            ],
            'sample_count': 15,
            'type': 'aggregated'
        }
        
        client.publish(topic, json.dumps(payload2))
        print(f"📤 Published aggregated data with 15 samples")
        
        time.sleep(1)
        client.loop_stop()
        client.disconnect()
        
        print("=" * 80)
        print("✓ Test publishing complete!")
        print("=" * 80)
        
    except Exception as e:
        print(f"✗ Error: {e}")
    
if __name__ == '__main__':
    publish_test_data()
