import paho.mqtt.client as mqtt
import json
from datetime import datetime

# MQTT Configuration for HiveMQ
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/processed_data"
CLIENT_ID = f"verification_subscriber_{int(datetime.now().timestamp())}"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to MQTT broker: {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        print(f"📡 Subscribed to topic: {MQTT_TOPIC}")
        print("🎧 Listening for messages... (Press Ctrl+C to exit)\n")
    else:
        print(f"❌ Failed to connect. Return code: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        print("="*60)
        print(f"📥 Message received at {datetime.now().strftime('%H:%M:%S')}")
        print(f"📋 Topic: {msg.topic}")
        print("📊 Data:")
        for key, value in data.items():
            print(f"   {key}: {value}")
        print("="*60 + "\n")
        
    except json.JSONDecodeError:
        print(f"📄 Received non-JSON message: {msg.payload.decode()}")
    except Exception as e:
        print(f"❌ Error processing message: {e}")

def on_disconnect(client, userdata, rc):
    print(f"❌ Disconnected from MQTT broker. Return code: {rc}")

if __name__ == "__main__":
    print("🚀 Starting MQTT subscriber for verification...")
    print(f"📡 Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"📋 Topic: {MQTT_TOPIC}")
    print(f"🆔 Client ID: {CLIENT_ID}")
    
    client = mqtt.Client(client_id=CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Shutting down subscriber...")
        client.disconnect()
    except Exception as e:
        print(f"❌ Error: {e}")
