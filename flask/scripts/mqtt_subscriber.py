#!/usr/bin/env python3
"""
MQTT Subscriber for EcoWatt Decompressed Data

Subscribes to MQTT topics and displays decompressed sensor data from ESP32 devices.
Topic pattern: ecowatt/data/{device_id}

Usage:
    python3 mqtt_subscriber.py                          # Subscribe to all devices
    python3 mqtt_subscriber.py --device ESP32_001       # Subscribe to specific device
    python3 mqtt_subscriber.py --broker localhost       # Use different broker
"""

import paho.mqtt.client as mqtt
import json
import argparse
import time
from datetime import datetime
import sys


class EcoWattSubscriber:
    def __init__(self, broker="broker.hivemq.com", port=1883, device_id=None):
        self.broker = broker
        self.port = port
        self.device_id = device_id
        self.client = None
        self.message_count = 0
        self.total_samples = 0
        
    def on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker: {self.broker}:{self.port}")
            print("=" * 80)
            
            # Subscribe to topic
            if self.device_id:
                topic = f"ecowatt/data/{self.device_id}"
                print(f"📡 Subscribing to: {topic}")
            else:
                topic = "ecowatt/data/#"
                print(f"📡 Subscribing to: {topic} (all devices)")
            
            client.subscribe(topic)
            print("=" * 80)
            print("Listening for messages... (Press Ctrl+C to stop)\n")
        else:
            print(f"✗ Connection failed with code {rc}")
            sys.exit(1)
    
    def on_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT broker"""
        if rc != 0:
            print(f"\n⚠ Unexpected disconnect (code: {rc})")
    
    def on_message(self, client, userdata, msg):
        """Callback when message received"""
        try:
            self.message_count += 1
            
            # Parse JSON payload
            payload = json.loads(msg.payload.decode())
            
            # Extract device info
            device_id = payload.get('device_id', 'unknown')
            timestamp = payload.get('timestamp', time.time())
            dt = datetime.fromtimestamp(timestamp)
            
            # Header
            print("=" * 80)
            print(f"📨 Message #{self.message_count} | Topic: {msg.topic}")
            print(f"🔌 Device: {device_id} | Time: {dt.strftime('%Y-%m-%d %H:%M:%S')}")
            print("-" * 80)
            
            # Check data type
            if 'values' in payload:
                # Decompressed data
                values = payload['values']
                sample_count = payload.get('sample_count', len(values))
                self.total_samples += sample_count
                
                print(f"📊 Type: Decompressed Data")
                print(f"📈 Samples: {sample_count}")
                
                # Show compression stats if available
                if 'compression_stats' in payload:
                    stats = payload['compression_stats']
                    print(f"🗜️  Compression Method: {stats.get('method', 'unknown')}")
                    print(f"🗜️  Compression Ratio: {stats.get('ratio', 0):.2%}")
                    print(f"🗜️  Original Size: {stats.get('original_size', 0)} bytes")
                    print(f"🗜️  Compressed Size: {stats.get('compressed_size', 0)} bytes")
                
                # Show sample values
                print(f"\n📋 Values (showing first 10 of {sample_count}):")
                for i, value in enumerate(values[:10]):
                    print(f"   [{i}] = {value}")
                
                if sample_count > 10:
                    print(f"   ... ({sample_count - 10} more values)")
                
            elif 'samples' in payload:
                # Aggregated data
                samples = payload['samples']
                sample_count = payload.get('sample_count', len(samples))
                self.total_samples += sample_count
                
                print(f"📊 Type: Aggregated Data")
                print(f"📈 Samples: {sample_count}")
                
                # Show sample data
                print(f"\n📋 Samples (showing first 5 of {sample_count}):")
                for i, sample in enumerate(samples[:5]):
                    voltage = sample.get('voltage', 'N/A')
                    current = sample.get('current', 'N/A')
                    power = sample.get('power', 'N/A')
                    sample_ts = sample.get('timestamp', 'N/A')
                    print(f"   [{i}] V={voltage}, I={current}, P={power}, ts={sample_ts}")
                
                if sample_count > 5:
                    print(f"   ... ({sample_count - 5} more samples)")
            else:
                print("⚠ Unknown data format")
                print(f"Raw payload: {payload}")
            
            print(f"\n📊 Session Stats: {self.message_count} messages, {self.total_samples} total samples")
            print("=" * 80)
            print()
            
        except json.JSONDecodeError as e:
            print(f"✗ JSON decode error: {e}")
            print(f"Raw payload: {msg.payload.decode()}")
        except Exception as e:
            print(f"✗ Error processing message: {e}")
            print(f"Raw payload: {msg.payload.decode()}")
    
    def start(self):
        """Start the MQTT subscriber"""
        try:
            # Create MQTT client
            client_id = f"ecowatt_subscriber_{int(time.time())}"
            self.client = mqtt.Client(client_id)
            
            # Set callbacks
            self.client.on_connect = self.on_connect
            self.client.on_disconnect = self.on_disconnect
            self.client.on_message = self.on_message
            
            print("=" * 80)
            print("🌐 EcoWatt MQTT Subscriber")
            print("=" * 80)
            print(f"Broker: {self.broker}:{self.port}")
            if self.device_id:
                print(f"Device Filter: {self.device_id}")
            else:
                print("Device Filter: All devices")
            print("=" * 80)
            
            # Connect to broker
            print(f"\n⏳ Connecting to {self.broker}:{self.port}...")
            self.client.connect(self.broker, self.port, 60)
            
            # Start loop
            self.client.loop_forever()
            
        except KeyboardInterrupt:
            print("\n\n" + "=" * 80)
            print("🛑 Subscriber stopped by user")
            print("=" * 80)
            print(f"📊 Final Stats:")
            print(f"   Messages received: {self.message_count}")
            print(f"   Total samples: {self.total_samples}")
            print("=" * 80)
            
        except Exception as e:
            print(f"\n✗ Error: {e}")
            sys.exit(1)
        
        finally:
            if self.client:
                self.client.disconnect()


def main():
    parser = argparse.ArgumentParser(
        description='Subscribe to EcoWatt MQTT topics for decompressed sensor data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 mqtt_subscriber.py
  python3 mqtt_subscriber.py --device TEST_ESP32_M3
  python3 mqtt_subscriber.py --broker localhost --port 1883
  python3 mqtt_subscriber.py --device ESP32_001 --broker broker.hivemq.com
        """
    )
    
    parser.add_argument(
        '--broker',
        type=str,
        default='broker.hivemq.com',
        help='MQTT broker address (default: broker.hivemq.com)'
    )
    
    parser.add_argument(
        '--port',
        type=int,
        default=1883,
        help='MQTT broker port (default: 1883)'
    )
    
    parser.add_argument(
        '--device',
        type=str,
        default=None,
        help='Specific device ID to subscribe to (default: all devices)'
    )
    
    args = parser.parse_args()
    
    # Create and start subscriber
    subscriber = EcoWattSubscriber(
        broker=args.broker,
        port=args.port,
        device_id=args.device
    )
    
    subscriber.start()


if __name__ == '__main__':
    main()
