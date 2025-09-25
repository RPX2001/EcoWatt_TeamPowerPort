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

def decompress_delta_data(compressed_string):
    """
    Decompress DELTA compressed data
    Format: "D:firstValue|delta1,delta2,delta3,...,"
    """
    try:
        if not compressed_string.startswith("D:"):
            return []
        
        # Remove "D:" prefix
        data = compressed_string[2:]
        pipe_pos = data.find('|')
        
        if pipe_pos == -1:
            return []
        
        # Get first value
        first_value = int(data[:pipe_pos])
        result = [first_value]
        
        # Process deltas
        deltas_str = data[pipe_pos + 1:]
        if deltas_str:
            delta_parts = deltas_str.split(',')
            current_value = first_value
            
            for delta_str in delta_parts:
                if delta_str.strip():  # Skip empty strings
                    delta = int(delta_str)
                    current_value = current_value + delta
                    result.append(current_value)
        
        return result
    except Exception as e:
        logger.error(f"Error decompressing delta data '{compressed_string}': {e}")
        return []

def process_register_data(registers, decompressed_values):
    """
    Process decompressed register data and map to meaningful names
    """
    if len(registers) != len(decompressed_values):
        logger.warning(f"Register count ({len(registers)}) doesn't match values count ({len(decompressed_values)})")
        return {}
    
    register_data = {}
    
    for i, reg_name in enumerate(registers):
        value = decompressed_values[i] if i < len(decompressed_values) else 0
        register_data[reg_name] = value
        
        # Add human-readable interpretations
        if reg_name == "REG_VAC1":
            register_data["ac_voltage_readable"] = f"{value/10.0:.1f}V"  # Assuming value is in 0.1V
        elif reg_name == "REG_IAC1":
            register_data["ac_current_readable"] = f"{value/100.0:.2f}A"  # Assuming value is in 0.01A
        elif reg_name == "REG_IPV1":
            register_data["pv_current_readable"] = f"{value/100.0:.2f}A"  # Assuming value is in 0.01A
        elif reg_name == "REG_PAC":
            register_data["ac_power_readable"] = f"{value}W"
    
        # Calculate efficiency if we have the right registers
        if "REG_VAC1" in register_data and "REG_IAC1" in register_data and "REG_PAC" in register_data:
            vac = register_data["REG_VAC1"]
            iac = register_data["REG_IAC1"]
            pac = register_data["REG_PAC"]
            
            if vac > 0 and iac > 0:
                # Convert units: VAC (0.1V), IAC (0.01A), PAC (W)
                vac_volts = vac / 10.0
                iac_amps = iac / 100.0
                apparent_power = vac_volts * iac_amps
                actual_power = pac
                if apparent_power > 0:
                    efficiency = round((actual_power / apparent_power) * 100, 2)
                    register_data["power_efficiency"] = efficiency
                    register_data["power_efficiency_readable"] = f"{efficiency}%"
    
    return register_data

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
        
        # Validate the expected format: {id, n, registers, data, compression}
        if not data or 'id' not in data or 'n' not in data or 'registers' not in data or 'data' not in data:
            return jsonify({'error': 'Invalid data format - expected {id, n, registers, data, compression}'}), 400
        
        device_id = data['id']
        entry_count = data['n']
        registers = data['registers']
        compressed_data_array = data['data']
        compression_info = data.get('compression', [])
        
        logger.info(f"Received from {device_id}: {entry_count} compressed entries with registers: {registers}")
        
        # Process each compressed data entry
        processed_entries = []
        total_power = 0
        
        for i, compressed_string in enumerate(compressed_data_array):
            # Decompress the data string
            decompressed_values = decompress_delta_data(compressed_string)
            
            # Process register values with decompressed data
            register_data = process_register_data(registers, decompressed_values)
            
            # Get compression info for this entry
            compression_details = compression_info[i] if i < len(compression_info) else {}
            
            # Verify decompressed values match compression info
            expected_values = compression_details.get('decompressed_values', [])
            if expected_values and expected_values != decompressed_values:
                logger.warning(f"Entry {i+1}: Decompressed values don't match expected values")
                logger.warning(f"  Decompressed: {decompressed_values}")
                logger.warning(f"  Expected: {expected_values}")
            
            processed_entry = {
                'entry_id': i + 1,
                'timestamp': compression_details.get('timestamp', 0),
                'compressed_string': compressed_string,
                'decompressed_values': decompressed_values,
                'register_mapping': dict(zip(registers, decompressed_values)),
                'register_data': register_data,
                'compression_info': {
                    'type': compression_details.get('type', 'unknown'),
                    'original_count': compression_details.get('original_count', len(decompressed_values)),
                    'compressed_string_length': len(compressed_string),
                    'decompression_successful': len(decompressed_values) > 0
                }
            }
            
            processed_entries.append(processed_entry)
            
            # Add to total power if REG_PAC exists
            if 'REG_PAC' in register_data:
                total_power += register_data['REG_PAC']
        
        # Create summary statistics
        avg_power = total_power / entry_count if entry_count > 0 else 0
        
        final_processed_data = {
            'device_id': device_id,
            'registers': registers,
            'batch_info': {
                'entry_count': entry_count,
                'total_power': total_power,
                'average_power': round(avg_power, 2),
                'data_collection_span_seconds': entry_count * 2,  # Assuming 2 seconds between polls
                'server_timestamp': int(time.time() * 1000),
                'server_datetime': datetime.now().isoformat(),
                'compression_method': 'DELTA',
                'decompression_successful': all(entry['compression_info']['decompression_successful'] for entry in processed_entries)
            },
            'entries': processed_entries,
            'processing_rule': 'ecowatt_delta_decompression_analysis'
        }
        
        # Try to publish to MQTT
        mqtt_published = False
        if mqtt_client and mqtt_connected:
            try:
                mqtt_payload = json.dumps(final_processed_data, indent=2)
                result = mqtt_client.publish(MQTT_TOPIC, mqtt_payload, qos=0)
                
                if result.rc == mqtt.MQTT_ERR_SUCCESS:
                    mqtt_published = True
                    # Log summary with readable values
                    sample_entry = processed_entries[0] if processed_entries else {}
                    sample_register_data = sample_entry.get('register_data', {})
                    voltage_readable = sample_register_data.get('ac_voltage_readable', 'N/A')
                    
                    logger.info(f"Published {entry_count} entries to MQTT - Sample: {voltage_readable}, Avg Power: {avg_power}W")
            except Exception as e:
                logger.error(f"MQTT publish error: {e}")
        
        # If MQTT failed, try to reconnect
        if not mqtt_connected:
            logger.info("Attempting MQTT reconnection...")
            init_mqtt()
        
        return jsonify({
            'status': 'success',
            'device_id': device_id,
            'registers': registers,
            'processed_entries': entry_count,
            'total_power': total_power,
            'average_power': avg_power,
            'compression_method': 'DELTA',
            'decompression_successful': all(entry['compression_info']['decompression_successful'] for entry in processed_entries),
            'mqtt_topic': MQTT_TOPIC,
            'mqtt_published': mqtt_published,
            'message': f'Successfully decompressed and processed {entry_count} DELTA compressed register entries'
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
        'supported_format': '{id: string, n: number, registers: [], data: [], compression: [{}]}',
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
    print(f"Expected ESP32 format: {{id, n, registers, data, compression}}")
    
    if init_mqtt():
        print("MQTT client initialized successfully")
    else:
        print("Warning: MQTT client failed to initialize")
    
    print(f"MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"MQTT Topic: {MQTT_TOPIC}")
    print("Flask server starting on http://10.40.99.2:5001")
    
    app.run(host='0.0.0.0', port=5001, debug=False)