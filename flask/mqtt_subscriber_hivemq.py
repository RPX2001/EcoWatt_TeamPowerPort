import paho.mqtt.client as mqtt
import json
from datetime import datetime

# MQTT Configuration for HiveMQ - Updated to match EcoWatt server
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"
CLIENT_ID = f"ecowatt_subscriber_{int(datetime.now().timestamp())}"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT broker: {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
        print("Listening for messages... (Press Ctrl+C to exit)\n")
    else:
        print(f"Failed to connect. Return code: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        print("="*80)
        print(f"EcoWatt Data received at {datetime.now().strftime('%H:%M:%S')}")
        print(f"Topic: {msg.topic}")
        print("="*80)
        
        # Display batch information
        if 'batch_info' in data:
            batch_info = data['batch_info']
            print(f"Device ID: {data.get('device_id', 'unknown')}")
            print(f"Batch Summary:")
            print(f"   Entries: {batch_info.get('entry_count', 0)}")
            print(f"   Total Power: {batch_info.get('total_power', 0)}W")
            print(f"   Average Power: {batch_info.get('average_power', 0)}W")
            print(f"   Collection Span: {batch_info.get('data_collection_span_seconds', 0)}s")
            print(f"   Server Time: {batch_info.get('server_datetime', 'unknown')}")
            print("-" * 60)
        
        # Display processed entries
        if 'entries' in data:
            entries = data['entries']
            print(f"Processing {len(entries)} entries:")
            
            for entry in entries[:3]:  # Show first 3 entries to avoid spam
                register_data = entry.get('register_data', {})
                compression = entry.get('compression', {})
                
                print(f"   Entry {entry.get('entry_id', '?')}:")
                print(f"      AC Voltage: {register_data.get('ac_voltage', 0)}V")
                print(f"      AC Current: {register_data.get('ac_current', 0)}A")
                print(f"      PV Current: {register_data.get('pv_current', 0)}A")
                print(f"      AC Power: {register_data.get('ac_power', 0)}W")
                print(f"      Efficiency: {register_data.get('power_efficiency', 0)}%")
                print(f"      Compression: {compression.get('type', 'unknown')} ({compression.get('compressed_size', 0)} bytes)")
            
            if len(entries) > 3:
                print(f"   ... and {len(entries) - 3} more entries")
        
        # Display raw data for debugging (only if small)
        elif len(str(data)) < 500:
            print("Raw Data:")
            for key, value in data.items():
                if key not in ['entries', 'batch_info']:
                    print(f"   {key}: {value}")
        
        print("="*80 + "\n")
        
    except json.JSONDecodeError:
        print(f"Received non-JSON message: {msg.payload.decode()[:200]}...")
    except Exception as e:
        print(f"Error processing message: {e}")

def on_disconnect(client, userdata, rc):
    print(f"Disconnected from MQTT broker. Return code: {rc}")

if __name__ == "__main__":
    print("Starting EcoWatt MQTT subscriber for monitoring...")
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Topic: {MQTT_TOPIC}")
    print(f"Client ID: {CLIENT_ID}")
    print("Monitoring EcoWatt power data from ESP32 devices...")
    print("Expected data: Compressed register arrays with power analysis")
    
    client = mqtt.Client(client_id=CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nShutting down EcoWatt subscriber...")
        client.disconnect()
    except Exception as e:
        print(f"Error: {e}")
