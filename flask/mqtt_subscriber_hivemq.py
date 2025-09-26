import paho.mqtt.client as mqtt
import json
from datetime import datetime

# MQTT Configuration for HiveMQ - Smart Selection enhanced
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"
CLIENT_ID = f"ecowatt_smart_subscriber_{int(datetime.now().timestamp())}"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT broker: {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
        print("Listening for Smart Selection messages... (Press Ctrl+C to exit)\n")
    else:
        print(f"Failed to connect. Return code: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        print("="*80)
        print(f"EcoWatt Smart Selection Data received at {datetime.now().strftime('%H:%M:%S')}")
        print(f"Topic: {msg.topic}")
        print("="*80)
        
        # Display Smart Selection summary
        if 'smart_compression_summary' in data:
            smart_info = data['smart_compression_summary']
            print(f"Device ID: {data.get('device_id', 'unknown')}")
            print(f"Registers: {data.get('registers', [])}")
            print(f"Smart Compression Summary:")
            print(f"   System Type: {smart_info.get('system_type', 'unknown')}")
            print(f"   Entries Processed: {smart_info.get('entry_count', 0)}")
            print(f"   Success Rate: {smart_info.get('decompression_success_rate', 0)*100:.1f}%")
            print(f"   Total Power: {smart_info.get('total_power_watts', 0)}W")
            print(f"   Average Power: {smart_info.get('average_power_watts', 0)}W")
            print(f"   Server Time: {smart_info.get('server_datetime', 'unknown')}")
            print("-" * 60)
        
        # Display processed entries with compression details
        if 'entries' in data:
            entries = data['entries']
            print(f"Processing {len(entries)} Smart Selection entries:")
            
            for entry in entries[:3]:  # Show first 3 entries
                register_data = entry.get('register_data', {})
                register_mapping = entry.get('register_mapping', {})
                compression_info = entry.get('smart_compression_info', {})
                
                print(f"   Entry {entry.get('entry_id', '?')} (Timestamp: {entry.get('timestamp', 0)}):")
                print(f"      Compression Method: {compression_info.get('method', 'unknown')}")
                print(f"      Academic Ratio: {compression_info.get('academic_ratio', 1.0):.3f}")
                print(f"      Storage Savings: {compression_info.get('compression_savings_percent', 0):.1f}%")
                print(f"      Compression Time: {compression_info.get('compression_time_us', 0)} μs")
                print(f"      Decompression: {'Success' if compression_info.get('decompression_successful', False) else 'Failed'}")
                print(f"      Decompressed Values: {entry.get('decompressed_values', [])}")
                print(f"      Register Mapping:")
                
                for reg, value in register_mapping.items():
                    readable_key = f"{reg.lower().replace('reg_', '')}_readable"
                    readable_value = register_data.get(readable_key, f"{value}")
                    print(f"         {reg}: {value} ({readable_value})")
                
                if 'power_efficiency' in register_data:
                    print(f"      Power Efficiency: {register_data.get('power_efficiency', 0)}%")
                
                print()  # Space between entries
            
            if len(entries) > 3:
                print(f"   ... and {len(entries) - 3} more entries")
        
        print("="*80 + "\n")
        
    except json.JSONDecodeError:
        print(f"Received non-JSON message: {msg.payload.decode()[:200]}...")
    except Exception as e:
        print(f"Error processing message: {e}")

def on_disconnect(client, userdata, rc):
    print(f"Disconnected from MQTT broker. Return code: {rc}")

if __name__ == "__main__":
    print("Starting EcoWatt Smart Selection MQTT Subscriber v3.1")
    print("="*60)
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Topic: {MQTT_TOPIC}")
    print(f"Client ID: {CLIENT_ID}")
    print("Monitoring Smart Selection compressed data from ESP32...")
    print("Expected: Dictionary+Bitmask (0.500 ratio), Temporal Delta, Semantic RLE, Bit-Packing")
    print("Academic Ratios: 0.500 (50% compression), Target: 0.21 (79% compression)")
    print("="*60)
    
    client = mqtt.Client(client_id=CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nShutting down Smart Selection subscriber...")
        client.disconnect()
    except Exception as e:
        print(f"Error: {e}")
