# from flask import Flask, request, jsonify
# import paho.mqtt.client as mqtt
# import json
# import time
# from datetime import datetime
# import threading
# import logging

# app = Flask(__name__)

# # Configure logging
# logging.basicConfig(level=logging.INFO)
# logger = logging.getLogger(__name__)

# # MQTT Configuration for HiveMQ public broker (more stable alternative)
# MQTT_BROKER = "broker.hivemq.com"
# MQTT_PORT = 1883
# MQTT_TOPIC = "esp32/processed_data"
# MQTT_CLIENT_ID = f"flask_server_{int(time.time())}"

# # Global MQTT client and connection status
# mqtt_client = None
# mqtt_connected = False

# def on_connect(client, userdata, flags, rc):
#     global mqtt_connected
#     if rc == 0:
#         mqtt_connected = True
#         logger.info(f"✅ Connected to MQTT broker at {MQTT_BROKER}")
#     else:
#         mqtt_connected = False
#         logger.error(f"❌ Failed to connect to MQTT broker. Return code: {rc}")

# def on_publish(client, userdata, mid):
#     logger.info(f"📤 Message {mid} published successfully")

# def on_disconnect(client, userdata, rc):
#     global mqtt_connected
#     mqtt_connected = False
#     logger.warning(f"❌ Disconnected from MQTT broker. Return code: {rc}")

# def init_mqtt():
#     global mqtt_client, mqtt_connected
    
#     mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
#     mqtt_client.on_connect = on_connect
#     mqtt_client.on_publish = on_publish
#     mqtt_client.on_disconnect = on_disconnect
    
#     try:
#         logger.info(f"🔄 Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
#         mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
#         mqtt_client.loop_start()
#         time.sleep(2)  # Wait for connection
#         return mqtt_connected
#     except Exception as e:
#         logger.error(f"❌ MQTT connection error: {e}")
#         return False

# @app.route('/process', methods=['POST'])
# def process_data():
#     try:
#         data = request.get_json()
        
#         if not data or 'number' not in data:
#             return jsonify({'error': 'Invalid data format'}), 400
        
#         original_number = data['number']
#         sensor_id = data.get('sensor_id', 'unknown')
#         esp32_timestamp = data.get('timestamp', 0)
        
#         logger.info(f"📥 Received from ESP32: number={original_number}, sensor={sensor_id}")
        
#         processed_number = original_number * 2
        
#         processed_data = {
#             'original_number': original_number,
#             'processed_number': processed_number,
#             'sensor_id': sensor_id,
#             'esp32_timestamp': esp32_timestamp,
#             'server_timestamp': int(time.time() * 1000),
#             'server_datetime': datetime.now().isoformat(),
#             'processing_rule': 'multiply_by_2'
#         }
        
#         # Try to publish to MQTT
#         if mqtt_client and mqtt_connected:
#             try:
#                 mqtt_payload = json.dumps(processed_data)
#                 result = mqtt_client.publish(MQTT_TOPIC, mqtt_payload, qos=0)  # QoS 0 for speed
                
#                 if result.rc == mqtt.MQTT_ERR_SUCCESS:
#                     logger.info(f"📤 Published to MQTT: {original_number} → {processed_number}")
#                     return jsonify({
#                         'status': 'success',
#                         'original_number': original_number,
#                         'processed_number': processed_number,
#                         'mqtt_topic': MQTT_TOPIC,
#                         'mqtt_published': True
#                     }), 200
#             except Exception as e:
#                 logger.error(f"❌ MQTT publish error: {e}")
        
#         # If MQTT failed, try to reconnect
#         if not mqtt_connected:
#             logger.info("🔄 Attempting MQTT reconnection...")
#             init_mqtt()
        
#         return jsonify({
#             'status': 'partial_success',
#             'original_number': original_number,
#             'processed_number': processed_number,
#             'mqtt_published': False,
#             'mqtt_error': 'MQTT connection issue'
#         }), 200
            
#     except Exception as e:
#         logger.error(f"❌ Error processing request: {e}")
#         return jsonify({'error': str(e)}), 500

# @app.route('/status', methods=['GET'])
# def get_status():
#     return jsonify({
#         'server_status': 'running',
#         'server_ip': '10.40.99.2',
#         'server_port': 5001,
#         'mqtt_broker': MQTT_BROKER,
#         'mqtt_connected': mqtt_connected,
#         'mqtt_topic': MQTT_TOPIC,
#         'current_time': datetime.now().isoformat()
#     })

# @app.route('/health', methods=['GET'])
# def health_check():
#     return jsonify({'status': 'healthy'}), 200

# if __name__ == '__main__':
#     print("🚀 Starting Flask server with HiveMQ broker...")
#     print(f"🌐 Server IP: 10.40.99.2")
#     print(f"🔌 Server Port: 5001")
    
#     if init_mqtt():
#         print("✅ MQTT client initialized successfully")
#     else:
#         print("⚠️  Warning: MQTT client failed to initialize")
    
#     print(f"📡 MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
#     print(f"📋 MQTT Topic: {MQTT_TOPIC}")
#     print("🌟 Flask server starting on http://10.40.99.2:5001")
    
#     app.run(host='0.0.0.0', port=5001, debug=False)
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
MQTT_TOPIC = "esp32/ecowatt_data"
MQTT_CLIENT_ID = f"flask_ecowatt_server_{int(time.time())}"

# Global MQTT client and connection status
mqtt_client = None
mqtt_connected = False

def on_connect(client, userdata, flags, rc):
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        logger.info(f"Connected to MQTT broker at {MQTT_BROKER}")
    else:
        mqtt_connected = False
        logger.error(f"Failed to connect to MQTT broker. Return code: {rc}")

def on_publish(client, userdata, mid):
    logger.info(f"Message {mid} published successfully")

def on_disconnect(client, userdata, rc):
    global mqtt_connected
    mqtt_connected = False
    logger.warning(f"Disconnected from MQTT broker. Return code: {rc}")

def init_mqtt():
    global mqtt_client, mqtt_connected
    
    mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_publish = on_publish
    mqtt_client.on_disconnect = on_disconnect
    
    try:
        logger.info(f"Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.loop_start()
        time.sleep(2)  # Wait for connection
        return mqtt_connected
    except Exception as e:
        logger.error(f"MQTT connection error: {e}")
        return False

def process_register_data(data_array):
    """
    Process register data from ESP32 - extract meaningful values
    Expected register order: [REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC]
    """
    if len(data_array) >= 4:
        return {
            'ac_voltage': data_array[0],      # REG_VAC1 - AC Voltage
            'ac_current': data_array[1],      # REG_IAC1 - AC Current  
            'pv_current': data_array[2],      # REG_IPV1 - PV Current
            'ac_power': data_array[3],        # REG_PAC - AC Power
            'power_efficiency': round((data_array[3] / max(data_array[0] * data_array[1], 1)) * 100, 2) if data_array[0] > 0 and data_array[1] > 0 else 0
        }
    return {}

@app.route('/process', methods=['POST'])
def process_compressed_data():
    try:
        data = request.get_json()
        
        # DEBUG: Print raw JSON received from ESP32
        print("="*80)
        print("DEBUG: Raw JSON received from ESP32:")
        print(json.dumps(data, indent=2))
        print("="*80)
        logger.info(f"Raw ESP32 payload: {json.dumps(data)}")
        
        # Validate the expected format: {id: {}, n: {}, data: {}, compression: {}}
        if not data or 'id' not in data or 'n' not in data or 'data' not in data:
            return jsonify({'error': 'Invalid data format - expected {id, n, data, compression}'}), 400
        
        device_id = data['id']
        entry_count = data['n']
        data_arrays = data['data']
        compression_info = data.get('compression', [])
        
        logger.info(f"Received from {device_id}: {entry_count} compressed entries")
        
        # Process each data entry
        processed_entries = []
        total_power = 0
        
        for i, data_array in enumerate(data_arrays):
            # Process register values
            register_data = process_register_data(data_array)
            
            # Get compression info for this entry
            compression_details = compression_info[i] if i < len(compression_info) else {}
            
            processed_entry = {
                'entry_id': i + 1,
                'raw_values': data_array,
                'register_data': register_data,
                'compression': {
                    'type': compression_details.get('type', 'unknown'),
                    'original_size': len(data_array) * 2,  # 2 bytes per uint16_t
                    'compressed_size': len(compression_details.get('compressed', '')),
                    'timestamp': compression_details.get('timestamp', 0)
                }
            }
            
            processed_entries.append(processed_entry)
            total_power += register_data.get('ac_power', 0)
        
        # Create summary statistics
        avg_power = total_power / entry_count if entry_count > 0 else 0
        
        final_processed_data = {
            'device_id': device_id,
            'batch_info': {
                'entry_count': entry_count,
                'total_power': total_power,
                'average_power': round(avg_power, 2),
                'data_collection_span_seconds': entry_count * 2,  # 2 seconds between polls
                'server_timestamp': int(time.time() * 1000),
                'server_datetime': datetime.now().isoformat()
            },
            'entries': processed_entries,
            'processing_rule': 'ecowatt_register_analysis'
        }
        
        # Try to publish to MQTT
        mqtt_published = False
        if mqtt_client and mqtt_connected:
            try:
                mqtt_payload = json.dumps(final_processed_data, indent=2)
                result = mqtt_client.publish(MQTT_TOPIC, mqtt_payload, qos=0)
                
                if result.rc == mqtt.MQTT_ERR_SUCCESS:
                    mqtt_published = True
                    logger.info(f"Published {entry_count} entries to MQTT - Avg Power: {avg_power}W")
            except Exception as e:
                logger.error(f"MQTT publish error: {e}")
        
        # If MQTT failed, try to reconnect
        if not mqtt_connected:
            logger.info("Attempting MQTT reconnection...")
            init_mqtt()
        
        return jsonify({
            'status': 'success',
            'device_id': device_id,
            'processed_entries': entry_count,
            'total_power': total_power,
            'average_power': avg_power,
            'mqtt_topic': MQTT_TOPIC,
            'mqtt_published': mqtt_published,
            'message': f'Successfully processed {entry_count} compressed register entries'
        }), 200
            
    except Exception as e:
        logger.error(f"Error processing compressed data: {e}")
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
        'supported_format': '{id: string, n: number, data: [[]], compression: [{}]}',
        'current_time': datetime.now().isoformat()
    })

@app.route('/health', methods=['GET'])
def health_check():
    return jsonify({'status': 'healthy'}), 200

# Legacy endpoint for backward compatibility
@app.route('/process_simple', methods=['POST'])
def process_simple_data():
    """Legacy endpoint for simple number processing"""
    try:
        data = request.get_json()
        
        if not data or 'number' not in data:
            return jsonify({'error': 'Invalid data format'}), 400
        
        original_number = data['number']
        processed_number = original_number * 2
        
        return jsonify({
            'status': 'success',
            'original_number': original_number,
            'processed_number': processed_number
        }), 200
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    print("Starting EcoWatt Flask Server with HiveMQ broker...")
    print(f"Server IP: 10.40.99.2")
    print(f"Server Port: 5001")
    print(f"Expected ESP32 format: {{id, n, data, compression}}")
    
    if init_mqtt():
        print("MQTT client initialized successfully")
    else:
        print("Warning: MQTT client failed to initialize")
    
    print(f"MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"MQTT Topic: {MQTT_TOPIC}")
    print("Flask server starting on http://10.40.99.2:5001")
    
    app.run(host='0.0.0.0', port=5001, debug=False)