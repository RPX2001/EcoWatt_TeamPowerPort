from flask import Flask, request, jsonify
import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime
import threading
import logging

app = Flask(__name__)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# MQTT Configuration for HiveMQ public broker (more stable alternative)
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/processed_data"
MQTT_CLIENT_ID = f"flask_server_{int(time.time())}"

# Global MQTT client and connection status
mqtt_client = None
mqtt_connected = False

def on_connect(client, userdata, flags, rc):
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        logger.info(f"✅ Connected to MQTT broker at {MQTT_BROKER}")
    else:
        mqtt_connected = False
        logger.error(f"❌ Failed to connect to MQTT broker. Return code: {rc}")

def on_publish(client, userdata, mid):
    logger.info(f"📤 Message {mid} published successfully")

def on_disconnect(client, userdata, rc):
    global mqtt_connected
    mqtt_connected = False
    logger.warning(f"❌ Disconnected from MQTT broker. Return code: {rc}")

def init_mqtt():
    global mqtt_client, mqtt_connected
    
    mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_publish = on_publish
    mqtt_client.on_disconnect = on_disconnect
    
    try:
        logger.info(f"🔄 Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.loop_start()
        time.sleep(2)  # Wait for connection
        return mqtt_connected
    except Exception as e:
        logger.error(f"❌ MQTT connection error: {e}")
        return False

@app.route('/process', methods=['POST'])
def process_data():
    try:
        data = request.get_json()
        
        if not data or 'number' not in data:
            return jsonify({'error': 'Invalid data format'}), 400
        
        original_number = data['number']
        sensor_id = data.get('sensor_id', 'unknown')
        esp32_timestamp = data.get('timestamp', 0)
        
        logger.info(f"📥 Received from ESP32: number={original_number}, sensor={sensor_id}")
        
        processed_number = original_number * 2
        
        processed_data = {
            'original_number': original_number,
            'processed_number': processed_number,
            'sensor_id': sensor_id,
            'esp32_timestamp': esp32_timestamp,
            'server_timestamp': int(time.time() * 1000),
            'server_datetime': datetime.now().isoformat(),
            'processing_rule': 'multiply_by_2'
        }
        
        # Try to publish to MQTT
        if mqtt_client and mqtt_connected:
            try:
                mqtt_payload = json.dumps(processed_data)
                result = mqtt_client.publish(MQTT_TOPIC, mqtt_payload, qos=0)  # QoS 0 for speed
                
                if result.rc == mqtt.MQTT_ERR_SUCCESS:
                    logger.info(f"📤 Published to MQTT: {original_number} → {processed_number}")
                    return jsonify({
                        'status': 'success',
                        'original_number': original_number,
                        'processed_number': processed_number,
                        'mqtt_topic': MQTT_TOPIC,
                        'mqtt_published': True
                    }), 200
            except Exception as e:
                logger.error(f"❌ MQTT publish error: {e}")
        
        # If MQTT failed, try to reconnect
        if not mqtt_connected:
            logger.info("🔄 Attempting MQTT reconnection...")
            init_mqtt()
        
        return jsonify({
            'status': 'partial_success',
            'original_number': original_number,
            'processed_number': processed_number,
            'mqtt_published': False,
            'mqtt_error': 'MQTT connection issue'
        }), 200
            
    except Exception as e:
        logger.error(f"❌ Error processing request: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/status', methods=['GET'])
def get_status():
    return jsonify({
        'server_status': 'running',
        'server_ip': '10.40.99.2',
        'server_port': 5001,
        'mqtt_broker': MQTT_BROKER,
        'mqtt_connected': mqtt_connected,
        'mqtt_topic': MQTT_TOPIC,
        'current_time': datetime.now().isoformat()
    })

@app.route('/health', methods=['GET'])
def health_check():
    return jsonify({'status': 'healthy'}), 200

if __name__ == '__main__':
    print("🚀 Starting Flask server with HiveMQ broker...")
    print(f"🌐 Server IP: 10.40.99.2")
    print(f"🔌 Server Port: 5001")
    
    if init_mqtt():
        print("✅ MQTT client initialized successfully")
    else:
        print("⚠️  Warning: MQTT client failed to initialize")
    
    print(f"📡 MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"📋 MQTT Topic: {MQTT_TOPIC}")
    print("🌟 Flask server starting on http://10.40.99.2:5001")
    
    app.run(host='0.0.0.0', port=5001, debug=False)
