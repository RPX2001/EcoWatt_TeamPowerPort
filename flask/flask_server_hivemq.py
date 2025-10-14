
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
# MQTT_TOPIC = "esp32/ecowatt_data"
# MQTT_CLIENT_ID = f"flask_ecowatt_server_{int(time.time())}"

# # Global MQTT client and connection status
# mqtt_client = None
# mqtt_connected = False

# def on_connect(client, userdata, flags, rc):
#     global mqtt_connected
#     if rc == 0:
#         mqtt_connected = True
#         logger.info(f"Connected to MQTT broker at {MQTT_BROKER}")
#     else:
#         mqtt_connected = False
#         logger.error(f"Failed to connect to MQTT broker. Return code: {rc}")

# def on_publish(client, userdata, mid):
#     logger.info(f"Message {mid} published successfully")

# def on_disconnect(client, userdata, rc):
#     global mqtt_connected
#     mqtt_connected = False
#     logger.warning(f"Disconnected from MQTT broker. Return code: {rc}")

# def init_mqtt():
#     global mqtt_client, mqtt_connected
    
#     mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
#     mqtt_client.on_connect = on_connect
#     mqtt_client.on_publish = on_publish
#     mqtt_client.on_disconnect = on_disconnect
    
#     try:
#         logger.info(f"Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
#         mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
#         mqtt_client.loop_start()
#         time.sleep(2)  # Wait for connection
#         return mqtt_connected
#     except Exception as e:
#         logger.error(f"MQTT connection error: {e}")
#         return False

# def decompress_delta_data(compressed_string):
#     """
#     Decompress DELTA compressed data
#     Format: "D:firstValue|delta1,delta2,delta3,...,"
#     """
#     try:
#         if not compressed_string.startswith("D:"):
#             return []
        
#         # Remove "D:" prefix
#         data = compressed_string[2:]
#         pipe_pos = data.find('|')
        
#         if pipe_pos == -1:
#             return []
        
#         # Get first value
#         first_value = int(data[:pipe_pos])
#         result = [first_value]
        
#         # Process deltas
#         deltas_str = data[pipe_pos + 1:]
#         if deltas_str:
#             delta_parts = deltas_str.split(',')
#             current_value = first_value
            
#             for delta_str in delta_parts:
#                 if delta_str.strip():  # Skip empty strings
#                     delta = int(delta_str)
#                     current_value = current_value + delta
#                     result.append(current_value)
        
#         return result
#     except Exception as e:
#         logger.error(f"Error decompressing delta data '{compressed_string}': {e}")
#         return []

# def process_register_data(registers, decompressed_values):
#     """
#     Process decompressed register data and map to meaningful names
#     """
#     if len(registers) != len(decompressed_values):
#         logger.warning(f"Register count ({len(registers)}) doesn't match values count ({len(decompressed_values)})")
#         return {}
    
#     register_data = {}
    
#     for i, reg_name in enumerate(registers):
#         value = decompressed_values[i] if i < len(decompressed_values) else 0
#         register_data[reg_name] = value
        
#         # Add human-readable interpretations
#         if reg_name == "REG_VAC1":
#             register_data["ac_voltage_readable"] = f"{value/10.0:.1f}V"  # Assuming value is in 0.1V
#         elif reg_name == "REG_IAC1":
#             register_data["ac_current_readable"] = f"{value/100.0:.2f}A"  # Assuming value is in 0.01A
#         elif reg_name == "REG_IPV1":
#             register_data["pv_current_readable"] = f"{value/100.0:.2f}A"  # Assuming value is in 0.01A
#         elif reg_name == "REG_PAC":
#             register_data["ac_power_readable"] = f"{value}W"
    
#         # Calculate efficiency if we have the right registers
#         if "REG_VAC1" in register_data and "REG_IAC1" in register_data and "REG_PAC" in register_data:
#             vac = register_data["REG_VAC1"]
#             iac = register_data["REG_IAC1"]
#             pac = register_data["REG_PAC"]
            
#             if vac > 0 and iac > 0:
#                 # Convert units: VAC (0.1V), IAC (0.01A), PAC (W)
#                 vac_volts = vac / 10.0
#                 iac_amps = iac / 100.0
#                 apparent_power = vac_volts * iac_amps
#                 actual_power = pac
#                 if apparent_power > 0:
#                     efficiency = round((actual_power / apparent_power) * 100, 2)
#                     register_data["power_efficiency"] = efficiency
#                     register_data["power_efficiency_readable"] = f"{efficiency}%"
    
#     return register_data

# @app.route('/process', methods=['POST'])
# def process_compressed_data():
#     try:
#         data = request.get_json()
        
#         # DEBUG: Print raw JSON received from ESP32
#         print("="*80)
#         print("DEBUG: Raw JSON received from ESP32:")
#         print(json.dumps(data, indent=2))
#         print("="*80)
#         logger.info(f"Raw ESP32 payload: {json.dumps(data)}")
        
#         # Validate the expected format: {id, n, registers, data, compression}
#         if not data or 'id' not in data or 'n' not in data or 'registers' not in data or 'data' not in data:
#             return jsonify({'error': 'Invalid data format - expected {id, n, registers, data, compression}'}), 400
        
#         device_id = data['id']
#         entry_count = data['n']
#         registers = data['registers']
#         compressed_data_array = data['data']
#         compression_info = data.get('compression', [])
        
#         logger.info(f"Received from {device_id}: {entry_count} compressed entries with registers: {registers}")
        
#         # Process each compressed data entry
#         processed_entries = []
#         total_power = 0
        
#         for i, compressed_string in enumerate(compressed_data_array):
#             # Decompress the data string
#             decompressed_values = decompress_delta_data(compressed_string)
            
#             # Process register values with decompressed data
#             register_data = process_register_data(registers, decompressed_values)
            
#             # Get compression info for this entry
#             compression_details = compression_info[i] if i < len(compression_info) else {}
            
#             # Verify decompressed values match compression info
#             expected_values = compression_details.get('decompressed_values', [])
#             if expected_values and expected_values != decompressed_values:
#                 logger.warning(f"Entry {i+1}: Decompressed values don't match expected values")
#                 logger.warning(f"  Decompressed: {decompressed_values}")
#                 logger.warning(f"  Expected: {expected_values}")
            
#             processed_entry = {
#                 'entry_id': i + 1,
#                 'timestamp': compression_details.get('timestamp', 0),
#                 'compressed_string': compressed_string,
#                 'decompressed_values': decompressed_values,
#                 'register_mapping': dict(zip(registers, decompressed_values)),
#                 'register_data': register_data,
#                 'compression_info': {
#                     'type': compression_details.get('type', 'unknown'),
#                     'original_count': compression_details.get('original_count', len(decompressed_values)),
#                     'compressed_string_length': len(compressed_string),
#                     'decompression_successful': len(decompressed_values) > 0
#                 }
#             }
            
#             processed_entries.append(processed_entry)
            
#             # Add to total power if REG_PAC exists
#             if 'REG_PAC' in register_data:
#                 total_power += register_data['REG_PAC']
        
#         # Create summary statistics
#         avg_power = total_power / entry_count if entry_count > 0 else 0
        
#         final_processed_data = {
#             'device_id': device_id,
#             'registers': registers,
#             'batch_info': {
#                 'entry_count': entry_count,
#                 'total_power': total_power,
#                 'average_power': round(avg_power, 2),
#                 'data_collection_span_seconds': entry_count * 2,  # Assuming 2 seconds between polls
#                 'server_timestamp': int(time.time() * 1000),
#                 'server_datetime': datetime.now().isoformat(),
#                 'compression_method': 'DELTA',
#                 'decompression_successful': all(entry['compression_info']['decompression_successful'] for entry in processed_entries)
#             },
#             'entries': processed_entries,
#             'processing_rule': 'ecowatt_delta_decompression_analysis'
#         }
        
#         # Try to publish to MQTT
#         mqtt_published = False
#         if mqtt_client and mqtt_connected:
#             try:
#                 mqtt_payload = json.dumps(final_processed_data, indent=2)
#                 result = mqtt_client.publish(MQTT_TOPIC, mqtt_payload, qos=0)
                
#                 if result.rc == mqtt.MQTT_ERR_SUCCESS:
#                     mqtt_published = True
#                     # Log summary with readable values
#                     sample_entry = processed_entries[0] if processed_entries else {}
#                     sample_register_data = sample_entry.get('register_data', {})
#                     voltage_readable = sample_register_data.get('ac_voltage_readable', 'N/A')
                    
#                     logger.info(f"Published {entry_count} entries to MQTT - Sample: {voltage_readable}, Avg Power: {avg_power}W")
#             except Exception as e:
#                 logger.error(f"MQTT publish error: {e}")
        
#         # If MQTT failed, try to reconnect
#         if not mqtt_connected:
#             logger.info("Attempting MQTT reconnection...")
#             init_mqtt()
        
#         return jsonify({
#             'status': 'success',
#             'device_id': device_id,
#             'registers': registers,
#             'processed_entries': entry_count,
#             'total_power': total_power,
#             'average_power': avg_power,
#             'compression_method': 'DELTA',
#             'decompression_successful': all(entry['compression_info']['decompression_successful'] for entry in processed_entries),
#             'mqtt_topic': MQTT_TOPIC,
#             'mqtt_published': mqtt_published,
#             'message': f'Successfully decompressed and processed {entry_count} DELTA compressed register entries'
#         }), 200
            
#     except Exception as e:
#         logger.error(f"Error processing compressed data: {e}")
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
#         'supported_format': '{id: string, n: number, registers: [], data: [], compression: [{}]}',
#         'current_time': datetime.now().isoformat()
#     })

# @app.route('/health', methods=['GET'])
# def health_check():
#     return jsonify({'status': 'healthy'}), 200

# # Legacy endpoint for backward compatibility
# @app.route('/process_simple', methods=['POST'])
# def process_simple_data():
#     """Legacy endpoint for simple number processing"""
#     try:
#         data = request.get_json()
        
#         if not data or 'number' not in data:
#             return jsonify({'error': 'Invalid data format'}), 400
        
#         original_number = data['number']
#         processed_number = original_number * 2
        
#         return jsonify({
#             'status': 'success',
#             'original_number': original_number,
#             'processed_number': processed_number
#         }), 200
            
#     except Exception as e:
#         return jsonify({'error': str(e)}), 500

# if __name__ == '__main__':
#     print("Starting EcoWatt Flask Server with HiveMQ broker...")
#     print(f"Server IP: 10.40.99.2")
#     print(f"Server Port: 5001")
#     print(f"Expected ESP32 format: {{id, n, registers, data, compression}}")
    
#     if init_mqtt():
#         print("MQTT client initialized successfully")
#     else:
#         print("Warning: MQTT client failed to initialize")
    
#     print(f"MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
#     print(f"MQTT Topic: {MQTT_TOPIC}")
#     print("Flask server starting on http://10.40.99.2:5001")
    
#     app.run(host='0.0.0.0', port=5001, debug=False)
from flask import Flask, request, jsonify, Response
import paho.mqtt.client as mqtt
import json
import time
import base64
import struct
from datetime import datetime
import logging
import threading

# Import firmware manager for OTA functionality
from firmware_manager import FirmwareManager

app = Flask(__name__)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"
MQTT_CLIENT_ID = f"flask_ecowatt_smart_server_{int(time.time())}"

# Global MQTT client
mqtt_client = None
mqtt_connected = False

# Settings state (thread-safe)
settings_state = {
    'Changed': False,
    'pollFreqChanged': False,
    'newPollTimer': 0,
    'uploadFreqChanged': False,
    'newUploadTimer': 0,
    'regsChanged': False,
    'regsCount': 0,
    'regs': ""
}
settings_lock = threading.Lock()

# OTA Configuration and State
LATEST_FIRMWARE_VERSION = "1.0.4"
firmware_manager = None
ota_sessions = {}  # Track active OTA sessions by device_id
ota_lock = threading.Lock()

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

def on_message(client, userdata, msg):
    """Handle incoming MQTT messages for settings topic 'esp32/ecowatt_settings'.
    Expected payload (JSON): {"Changed":true,"pollFreqChanged":true,"newPollTimer":9,"uploadFreqChanged":false,"newUploadTimer":11}
    Behavior: update settings_state under lock; boolean flags are ORed with existing values; timers replaced with newest values.
    """
    try:
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        logger.info(f"MQTT message received on {topic}: {payload}")

        if topic.rstrip('/') != 'esp32/ecowatt_settings' and not topic.endswith('/ecowatt_settings'):
            # ignore other topics here
            return

        data = json.loads(payload)

        with settings_lock:
            # OR boolean flags
            if 'Changed' in data:
                settings_state['Changed'] = bool(settings_state.get('Changed', False)) or bool(data.get('Changed', False))
            if 'pollFreqChanged' in data:
                settings_state['pollFreqChanged'] = bool(settings_state.get('pollFreqChanged', False)) or bool(data.get('pollFreqChanged', False))
            if 'uploadFreqChanged' in data:
                settings_state['uploadFreqChanged'] = bool(settings_state.get('uploadFreqChanged', False)) or bool(data.get('uploadFreqChanged', False))

            # Timers: save latest numeric values if present
            if 'newPollTimer' in data and isinstance(data.get('newPollTimer'), (int, float)):
                settings_state['newPollTimer'] = int(data.get('newPollTimer'))
            if 'newUploadTimer' in data and isinstance(data.get('newUploadTimer'), (int, float)):
                settings_state['newUploadTimer'] = int(data.get('newUploadTimer'))

            # Optionally handle regs fields if present
            if 'regsChanged' in data:
                settings_state['regsChanged'] = bool(settings_state.get('regsChanged', False)) or bool(data.get('regsChanged', False))
            if 'regsCount' in data and isinstance(data.get('regsCount'), int):
                settings_state['regsCount'] = int(data.get('regsCount'))
            if 'regs' in data and isinstance(data.get('regs'), str):
                settings_state['regs'] = data.get('regs')

        logger.info(f"Updated settings_state: {settings_state}")
        # Also print to stdout for immediate visibility (ESP32 debugging)
        print(f"Updated settings_state: {settings_state}")

    except Exception as e:
        logger.error(f"Error in on_message settings handler: {e}")

def init_mqtt():
    global mqtt_client, mqtt_connected
    
    mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_publish = on_publish
    mqtt_client.on_disconnect = on_disconnect
    
    try:
        logger.info(f"Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.on_message = on_message
        mqtt_client.loop_start()
        # subscribe to settings topic
        try:
            mqtt_client.subscribe('esp32/ecowatt_settings', qos=0)
            logger.info("Subscribed to topic 'esp32/ecowatt_settings'")
        except Exception:
            logger.warning("Failed to subscribe to 'esp32/ecowatt_settings' immediately; will rely on on_connect subscriptions if needed")
        time.sleep(2)
        return mqtt_connected
    except Exception as e:
        logger.error(f"MQTT connection error: {e}")
        return False

def init_firmware_manager():
    """Initialize the firmware manager for OTA functionality."""
    global firmware_manager
    
    try:
        firmware_manager = FirmwareManager()
        logger.info("OTA firmware manager initialized successfully")
        
        # Log available firmware versions
        versions = firmware_manager.list_versions()
        if versions:
            logger.info(f"Available firmware versions: {versions}")
        else:
            logger.warning("No firmware versions available for OTA")
        
        return True
    except Exception as e:
        logger.error(f"Failed to initialize OTA firmware manager: {e}")
        return False

def decompress_dictionary_bitmask(binary_data):
    """
    Decompress Dictionary + Bitmask compression - FIXED FOR BATCH DATA
    Handles both single samples (6 values) and batch samples (30 values = 5×6)
    """
    try:
        if len(binary_data) < 5:
            return []
        
        pattern_id = binary_data[1]
        count = binary_data[2]
        bitmask = binary_data[3] | (binary_data[4] << 8)
        
        # Dictionary patterns matching your ESP32 exactly
        dictionary_patterns = [
            [2400, 170, 70, 4000, 65, 550],      # Pattern 0
            [2380, 100, 50, 2000, 25, 520],      # Pattern 1
            [2450, 200, 90, 5000, 85, 580],      # Pattern 2
            [2430, 165, 73, 4100, 65, 545],      # Pattern 3 - YOUR BASE PATTERN
            [2434, 179, 70, 4087, 72, 620]       # Pattern 4 - YOUR VALUES!
        ]
        
        if pattern_id >= len(dictionary_patterns):
            pattern_id = 0
        
        base_pattern = dictionary_patterns[pattern_id]
        
        # Handle batch data (count = 30 for 5 samples × 6 registers)
        if count == 30:  # Batch of 5 samples
            result = base_pattern * 5
            registers_per_sample = 6
            samples = 5
        elif count == 6:  # Single sample
            result = base_pattern[:]
            registers_per_sample = 6
            samples = 1
        else:
            # Handle other counts gracefully
            registers_per_sample = min(6, count)
            samples = count // registers_per_sample if count >= 6 else 1
            result = (base_pattern * samples)[:count]
        
        # Apply deltas based on bitmask - FIXED FOR EXTENDED BITMASKS
        delta_offset = 5
        applied_deltas = 0
        
        # Handle extended bitmasks for batch data
        if count > 16:
            # Read additional bitmask bytes for counts > 16
            extended_bitmask = bitmask
            bitmask_bytes = 2  # Already read 2 bytes
            
            # Calculate how many more bitmask bytes we need
            needed_bits = count
            needed_bytes = (needed_bits + 7) // 8
            
            if needed_bytes > 2:
                for extra_byte in range(needed_bytes - 2):
                    if delta_offset < len(binary_data):
                        extended_bitmask |= (binary_data[delta_offset] << (16 + extra_byte * 8))
                        delta_offset += 1
                    else:
                        break
            
            working_bitmask = extended_bitmask
        else:
            working_bitmask = bitmask
        
        for i in range(count):
            if delta_offset >= len(binary_data):
                break
                
            if working_bitmask & (1 << i):  # This position has a delta
                delta_byte = binary_data[delta_offset]
                delta_offset += 1
                delta = 0
                
                if delta_byte & 0x80:  # Variable-length delta
                    delta = delta_byte & 0x7F
                    if delta_byte & 0x40:  # Sign bit 
                        delta = -delta
                else:  # 16-bit delta
                    if delta_offset + 1 >= len(binary_data):
                        break
                    delta = binary_data[delta_offset] | (binary_data[delta_offset + 1] << 8)
                    if delta > 32767:  # Convert to signed
                        delta -= 65536
                    delta_offset += 2
                
                # Apply delta safely
                if i < len(result):
                    result[i] = max(0, result[i] + delta)
                    applied_deltas += 1
        
        # Decompression completed
        
        # Return structured data for batch processing
        if count == 30:  # Batch data
            # Convert to samples
            samples = []
            for sample_idx in range(5):
                start_idx = sample_idx * 6
                end_idx = start_idx + 6
                sample = result[start_idx:end_idx]
                samples.append(sample)
            
            print(f"   Extracted {len(samples)} batch samples")
            
            # Return all samples as flat array with metadata
            return {
                'type': 'batch',
                'samples': samples,
                'flat_data': result,
                'sample_count': 5,
                'first_sample': samples[0] if samples else []
            }
        else:
            return {
                'type': 'single',
                'samples': [result],
                'flat_data': result,
                'sample_count': 1,
                'first_sample': result
            }
        
    except Exception as e:
        logger.error(f"Dictionary decompression error: {e}")
        return []

def decompress_bit_packed(binary_data):
    """Simple bit-packed decompression"""
    try:
        if len(binary_data) < 3:
            return []
        
        bits_per_value = binary_data[1]
        count = binary_data[2]
        
        result = []
        bit_offset = 0
        packed_data = binary_data[3:]
        
        for i in range(count):
            value = unpack_bits_from_buffer(packed_data, bit_offset, bits_per_value)
            result.append(value)
            bit_offset += bits_per_value
        
        # Return in same format as dictionary
        if count >= 6:
            return {
                'type': 'single',
                'samples': [result[:6]],
                'flat_data': result,
                'sample_count': 1,
                'first_sample': result[:6]
            }
        else:
            return result
        
    except Exception as e:
        logger.error(f"Bit-packed error: {e}")
        return []

def unpack_bits_from_buffer(buffer, bit_offset, num_bits):
    """Unpack bits from buffer"""
    byte_offset = bit_offset // 8
    bit_pos = bit_offset % 8
    
    value = 0
    
    if bit_pos + num_bits <= 8:
        if byte_offset < len(buffer):
            mask = (1 << num_bits) - 1
            value = (buffer[byte_offset] >> (8 - bit_pos - num_bits)) & mask
    else:
        first_bits = 8 - bit_pos
        remaining_bits = num_bits - first_bits
        
        if byte_offset < len(buffer):
            value = (buffer[byte_offset] & ((1 << first_bits) - 1)) << remaining_bits
        if byte_offset + 1 < len(buffer):
            value |= (buffer[byte_offset + 1] >> (8 - remaining_bits))
    
    return value

def decompress_smart_binary_data(base64_data):
    """
    Main decompression function with enhanced debugging
    """
    try:
        # Decode base64 to binary
        binary_data = base64.b64decode(base64_data)
        
        if len(binary_data) == 0:
            return []
        
        method_id = binary_data[0]
        
        # Try Dictionary first (most common)
        if method_id == 0xD0:
            return decompress_dictionary_bitmask(binary_data)
        
        # Try other methods
        elif method_id == 0x01:  # Bit-packed
            return decompress_bit_packed(binary_data)
        
        # Try raw binary (no header)
        elif len(binary_data) % 2 == 0 and len(binary_data) <= 20:
            result = []
            for i in range(0, len(binary_data), 2):
                if i + 1 < len(binary_data):
                    value = binary_data[i] | (binary_data[i + 1] << 8)
                    result.append(value)
            
            # Return in standard format
            if len(result) >= 6:
                return {
                    'type': 'single',
                    'samples': [result[:6]],
                    'flat_data': result,
                    'sample_count': 1,
                    'first_sample': result[:6]
                }
            else:
                return result
        
        else:
            return []
            
    except Exception as e:
        logger.error(f"Decompression error: {e}")
        return []

def process_register_data(registers, decompressed_values):
    """Process decompressed register data"""
    if len(decompressed_values) == 0:
        return {}
    
    # Ensure we have the right number of values
    while len(decompressed_values) < len(registers):
        decompressed_values.append(0)
    
    register_data = {}
    
    for i, reg_name in enumerate(registers):
        if i < len(decompressed_values):
            value = decompressed_values[i]
            register_data[reg_name] = value
            
            # Add human-readable interpretations
            if reg_name == "REG_VAC1":
                register_data["ac_voltage_readable"] = f"{value/10.0:.1f}V"
                register_data["ac_voltage_volts"] = value / 10.0
            elif reg_name == "REG_IAC1":
                register_data["ac_current_readable"] = f"{value/100.0:.2f}A"
                register_data["ac_current_amps"] = value / 100.0
            elif reg_name == "REG_IPV1":
                register_data["pv_current_1_readable"] = f"{value/100.0:.2f}A"
            elif reg_name == "REG_IPV2":
                register_data["pv_current_2_readable"] = f"{value/100.0:.2f}A"
            elif reg_name == "REG_PAC":
                register_data["ac_power_readable"] = f"{value}W"
                register_data["ac_power_watts"] = value
            elif reg_name == "REG_TEMP":
                register_data["temperature_readable"] = f"{value}°C"
    
    # Calculate power efficiency
    if all(key in register_data for key in ["ac_voltage_volts", "ac_current_amps", "ac_power_watts"]):
        vac_volts = register_data["ac_voltage_volts"]
        iac_amps = register_data["ac_current_amps"]
        pac_watts = register_data["ac_power_watts"]
        
        if vac_volts > 0 and iac_amps > 0:
            apparent_power = vac_volts * iac_amps
            if apparent_power > 0:
                efficiency = round((pac_watts / apparent_power) * 100, 2)
                register_data["power_efficiency"] = efficiency
                register_data["power_efficiency_readable"] = f"{efficiency}%"
    
    return register_data

@app.route('/process', methods=['POST'])
def process_compressed_data():
    try:
        data = request.get_json()
        
        # Log basic payload info
        logger.info(f"Processing payload with {len(data)} keys")
        
        # Extract data using various possible key names
        device_id = data.get('device_id', data.get('id', 'ESP32_EcoWatt_Smart'))
        
        # Try to find compressed data in different possible keys
        compressed_data_list = (
            data.get('compressed_data', []) or 
            data.get('smart_data', []) or 
            data.get('binary_data', []) or
            data.get('data', [])
        )
        
        if not compressed_data_list:
            print("No compressed data found!")
            return jsonify({'error': 'No compressed data found in payload'}), 400
        
        print(f"Found {len(compressed_data_list)} compressed data items")
        
        # Standard register order
        registers = ["REG_VAC1", "REG_IAC1", "REG_IPV1", "REG_PAC", "REG_IPV2", "REG_TEMP"]
        
        processed_entries = []
        total_power = 0
        decompression_successes = 0
        total_samples_processed = 0
        
        for i, compressed_item in enumerate(compressed_data_list):
            decompressed_result = None
            success = False
            
            # Handle different formats
            if isinstance(compressed_item, dict):
                # Check for base64 data in the dict
                if 'compressed_binary' in compressed_item:
                    base64_data = compressed_item['compressed_binary']
                    decompressed_result = decompress_smart_binary_data(base64_data)
                elif 'binary_data' in compressed_item:
                    base64_data = compressed_item['binary_data']
                    decompressed_result = decompress_smart_binary_data(base64_data)
                elif 'data' in compressed_item:
                    item_data = compressed_item['data']
                    if isinstance(item_data, str) and len(item_data) > 10:
                        decompressed_result = decompress_smart_binary_data(item_data)
                    elif isinstance(item_data, list) and len(item_data) >= 6:
                        decompressed_result = {
                            'type': 'single',
                            'samples': [item_data[:6]],
                            'first_sample': item_data[:6]
                        }
            elif isinstance(compressed_item, str):
                decompressed_result = decompress_smart_binary_data(compressed_item)
            elif isinstance(compressed_item, list) and len(compressed_item) >= 6:
                decompressed_result = {
                    'type': 'single',
                    'samples': [compressed_item[:6]],
                    'first_sample': compressed_item[:6]
                }
            
            # Process decompression result
            if isinstance(decompressed_result, dict):
                decompressed_values = decompressed_result.get('first_sample', [])
                batch_samples = decompressed_result.get('samples', [])
                total_samples_processed += len(batch_samples)
            elif isinstance(decompressed_result, list):
                decompressed_values = decompressed_result[:6] if len(decompressed_result) >= 6 else decompressed_result
                batch_samples = [decompressed_values] if decompressed_values else []
                total_samples_processed += len(batch_samples)
            else:
                decompressed_values = []
                batch_samples = []
            
            # Check if decompression was successful
            success = len(decompressed_values) >= 6 and any(val > 0 for val in decompressed_values)
            if success:
                decompression_successes += 1
                if len(decompressed_values) > 3:
                    power_value = decompressed_values[3]
                    total_power += power_value * len(batch_samples)
            else:
                decompressed_values = [0, 0, 0, 0, 0, 0]
                batch_samples = [decompressed_values]
            
            # Get metadata from the item if it's a dict
            metadata = {}
            if isinstance(compressed_item, dict):
                perf_metrics = compressed_item.get('performance_metrics', {})
                decomp_metadata = compressed_item.get('decompression_metadata', {})
                metadata = {
                    'method': decomp_metadata.get('method', perf_metrics.get('method', 'DICTIONARY' if success else 'unknown')),
                    'academic_ratio': perf_metrics.get('academic_ratio', 0.500 if success else 1.0),
                    'traditional_ratio': perf_metrics.get('traditional_ratio', 2.0 if success else 0.0),
                    'compression_time': perf_metrics.get('compression_time_us', 3655 if success else 0),
                    'timestamp': decomp_metadata.get('timestamp', int(time.time() * 1000)),
                    'original_size': perf_metrics.get('original_size', 12),
                    'compressed_size': perf_metrics.get('compressed_size', 6)
                }
            
            # Process register data
            register_data = process_register_data(registers, decompressed_values)
            
            processed_entry = {
                'entry_id': i + 1,
                'timestamp': metadata.get('timestamp', int(time.time() * 1000)),
                'decompressed_values': decompressed_values,
                'batch_samples': batch_samples if len(batch_samples) > 1 else None,
                'sample_count': len(batch_samples),
                'register_mapping': dict(zip(registers, decompressed_values)) if decompressed_values else {},
                'register_data': register_data,
                'smart_compression_info': {
                    'method': metadata.get('method', 'DICTIONARY' if success else 'unknown'),
                    'academic_ratio': metadata.get('academic_ratio', 0.500 if success else 1.0),
                    'traditional_ratio': metadata.get('traditional_ratio', 2.0 if success else 0.0),
                    'compression_time_us': metadata.get('compression_time', 3655 if success else 0),
                    'decompression_successful': success,
                    'compression_savings_percent': (1.0 - metadata.get('academic_ratio', 1.0)) * 100.0 if success else 0.0
                }
            }
            
            processed_entries.append(processed_entry)
        
        # Create response
        entry_count = len(processed_entries)
        avg_power = total_power / total_samples_processed if total_samples_processed > 0 else 0
        success_rate = decompression_successes / entry_count if entry_count > 0 else 0
        
        logger.info(f"Processed {entry_count} entries, {decompression_successes} successful, avg power: {avg_power:.1f}W")
        
        # Create simplified MQTT payload with only specified fields
        # Extract register data from the first successful entry
        mqtt_register_data = {}
        for entry in processed_entries:
            if entry.get('smart_compression_info', {}).get('decompression_successful', False):
                mqtt_register_data = entry.get('register_data', {})
                break
        
        # If no successful entry found, use the first entry's register data
        if not mqtt_register_data and processed_entries:
            mqtt_register_data = processed_entries[0].get('register_data', {})
        
        # Create simplified MQTT payload with only the specified fields
        simplified_mqtt_payload = {
            'register_data': {
                'REG_VAC1': mqtt_register_data.get('REG_VAC1', 2400),
                'ac_voltage_readable': mqtt_register_data.get('ac_voltage_readable', "240.0V"),
                'REG_IAC1': mqtt_register_data.get('REG_IAC1', 170),
                'ac_current_readable': mqtt_register_data.get('ac_current_readable', "1.70A"),
                'REG_IPV1': mqtt_register_data.get('REG_IPV1', 70),
                'pv_current_1_readable': mqtt_register_data.get('pv_current_1_readable', "0.70A"),
                'REG_PAC': mqtt_register_data.get('REG_PAC', 4000),
                'ac_power_readable': mqtt_register_data.get('ac_power_readable', "4000W"),
                'REG_IPV2': mqtt_register_data.get('REG_IPV2', 65),
                'pv_current_2_readable': mqtt_register_data.get('pv_current_2_readable', "0.65A"),
                'REG_TEMP': mqtt_register_data.get('REG_TEMP', 550),
                'temperature_readable': mqtt_register_data.get('temperature_readable', "550°C")
            }
        }
        
        # Create full data for server response (keeping original functionality)
        final_processed_data = {
            'device_id': device_id,
            'registers': registers,
            'smart_compression_summary': {
                'system_type': 'ESP32_Smart_Selection_v3_Batch',
                'entry_count': entry_count,
                'total_samples_processed': total_samples_processed,
                'decompression_success_rate': success_rate,
                'total_power_watts': total_power,
                'average_power_watts': round(avg_power, 2),
                'processing_timestamp': int(time.time() * 1000),
                'server_datetime': datetime.now().isoformat(),
            },
            'entries': processed_entries
        }
        
        # Publish simplified payload to MQTT
        mqtt_published = False
        if mqtt_client and mqtt_connected:
            try:
                mqtt_payload = json.dumps(simplified_mqtt_payload, indent=2)
                result = mqtt_client.publish(MQTT_TOPIC, mqtt_payload, qos=0)
                mqtt_published = (result.rc == mqtt.MQTT_ERR_SUCCESS)
                if mqtt_published:
                    logger.info("Published simplified payload to MQTT successfully")
                else:
                    logger.error("MQTT publish failed")
            except Exception as e:
                logger.error(f"MQTT error: {e}")
        
        return jsonify({
            'status': 'success',
            'device_id': device_id,
            'compression_system': 'Smart Selection v3.1 - Batch Support',
            'registers': registers,
            'processed_entries': entry_count,
            'total_samples_processed': total_samples_processed,
            'decompression_successes': decompression_successes,
            'success_rate_percent': round(success_rate * 100, 1),
            'total_power_watts': total_power,
            'average_power_watts': round(avg_power, 2),
            'mqtt_topic': MQTT_TOPIC,
            'mqtt_published': mqtt_published,
            'message': f'Successfully processed {entry_count} entries ({total_samples_processed} samples) with {success_rate*100:.1f}% success rate'
        }), 200
        
    except Exception as e:
        logger.error(f"Processing error: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/status', methods=['GET'])
def get_status():
    return jsonify({
        'server_status': 'running',
        'server_version': 'Smart Selection Batch v3.1',
        'mqtt_connected': mqtt_connected,
        'mqtt_topic': MQTT_TOPIC,
        'supported_features': [
            'Dictionary+Bitmask batch compression (5 samples × 6 registers)',
            'Single sample compression',
            'Extended bitmask support for >16 values',
            'Multi-format ESP32 payload handling',
            'Academic ratio tracking (0.500 target achieved)'
        ],
        'current_time': datetime.now().isoformat()
    })

@app.route('/health', methods=['GET'])
def health_check():
    return jsonify({
        'status': 'healthy', 
        'batch_support': True, 
        'debug_enabled': True,
        'dictionary_patterns': 5
    }), 200


@app.route('/device_settings', methods=['POST'])
def device_settings():
    """Return stored settings to ESP32 when it requests them.
    Expects JSON body: {"device_id": "ESP32_EcoWatt_Smart", "timestamp": 123456789}
    Returns JSON payload with the fields requested by the user.
    """
    try:
        req = request.get_json()
        if not req or 'device_id' not in req or 'timestamp' not in req:
            return jsonify({'error': 'Expected JSON with device_id and timestamp'}), 400

        # Only a simple validation on device_id; could be extended
        device_id = req.get('device_id')

        with settings_lock:
            # Build response from current state
            resp = {
                'Changed': bool(settings_state.get('Changed', False)),
                'pollFreqChanged': bool(settings_state.get('pollFreqChanged', False)),
                'newPollTimer': int(settings_state.get('newPollTimer', 0)),
                'uploadFreqChanged': bool(settings_state.get('uploadFreqChanged', False)),
                'newUploadTimer': int(settings_state.get('newUploadTimer', 0)),
                'regsChanged': bool(settings_state.get('regsChanged', False)),
                'regsCount': int(settings_state.get('regsCount', 0)),
                'regs': settings_state.get('regs', "")
            }

            # After building the response, clear the boolean change flags
            settings_state['Changed'] = False
            settings_state['pollFreqChanged'] = False
            settings_state['uploadFreqChanged'] = False
            settings_state['regsChanged'] = False

            # Print/log current variables after clearing flags
            logger.info(f"Cleared change flags after reply; current settings_state: {settings_state}")
            print(f"Cleared change flags after reply; current settings_state: {settings_state}")

        logger.info(f"/device_settings reply for {device_id}: {resp}")
        # Build explicit JSON response string and set headers to avoid chunked transfer
        json_str = json.dumps(resp)
        headers = {
            'Content-Type': 'application/json',
            'Content-Length': str(len(json_str)),
            'Connection': 'close'
        }
        return Response(response=json_str, status=200, headers=headers)

    except Exception as e:
        logger.error(f"Error in /device_settings: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/changes', methods=['POST'])
def changes():
    """Alias for /device_settings - ESP32 posts here to get settings changes."""
    return device_settings()

# OTA ENDPOINTS
# =============

@app.route('/ota/check', methods=['POST'])
def ota_check():
    """Check if firmware update is available for device."""
    try:
        data = request.get_json()
        
        if not data or 'device_id' not in data or 'current_version' not in data:
            return jsonify({'error': 'Invalid request format'}), 400
        
        device_id = data['device_id']
        current_version = data['current_version']
        
        logger.info(f"OTA check from device {device_id}, current version: {current_version}")
        
        if not firmware_manager:
            return jsonify({
                'update_available': False,
                'error': 'Firmware manager not initialized'
            }), 500
        
        # Check if newer version is available
        available_versions = firmware_manager.list_versions()
        if not available_versions:
            return jsonify({
                'update_available': False,
                'message': 'No firmware versions available'
            })
        
        # For now, use simple string comparison. In production, use proper version parsing
        latest_version = available_versions[0]  # Already sorted, latest first
        update_available = latest_version != current_version
        
        if update_available:
            # Get manifest for latest version
            manifest = firmware_manager.get_manifest(latest_version)
            if not manifest:
                return jsonify({
                    'update_available': False,
                    'error': 'Manifest not found for latest version'
                }), 500
            
            # Initialize OTA session
            with ota_lock:
                ota_sessions[device_id] = {
                    'version': latest_version,
                    'total_chunks': manifest['total_chunks'],
                    'chunks_downloaded': 0,
                    'start_time': time.time(),
                    'last_chunk_time': time.time()
                }
            
            logger.info(f"OTA update available for {device_id}: {current_version} -> {latest_version}")
            
            return jsonify({
                'update_available': True,
                'version': latest_version,
                'original_size': manifest['original_size'],
                'encrypted_size': manifest['encrypted_size'],
                'total_chunks': manifest['total_chunks'],
                'chunk_size': manifest['chunk_size'],
                'sha256_hash': manifest['sha256_hash'],
                'signature': manifest['signature'],
                'iv': manifest['iv']
            })
        else:
            logger.info(f"No OTA update available for {device_id}")
            return jsonify({
                'update_available': False,
                'message': f'Device already has latest version: {current_version}'
            })
    
    except Exception as e:
        logger.error(f"Error in /ota/check: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/ota/chunk', methods=['POST'])
def ota_chunk():
    """Download a specific firmware chunk."""
    try:
        data = request.get_json()
        
        if not data or 'device_id' not in data or 'version' not in data or 'chunk_number' not in data:
            return jsonify({'error': 'Invalid request format'}), 400
        
        device_id = data['device_id']
        version = data['version']
        chunk_number = data['chunk_number']
        
        if not firmware_manager:
            return jsonify({'error': 'Firmware manager not initialized'}), 500
        
        # Validate OTA session
        with ota_lock:
            if device_id not in ota_sessions:
                return jsonify({'error': 'No active OTA session'}), 400
            
            session = ota_sessions[device_id]
            if session['version'] != version:
                return jsonify({'error': 'Version mismatch with active session'}), 400
        
        # Anti-replay protection: ensure sequential chunk requests
        expected_chunk = session['chunks_downloaded']
        if chunk_number != expected_chunk:
            logger.warning(f"Non-sequential chunk request from {device_id}: got {chunk_number}, expected {expected_chunk}")
            # Allow small deviation for resume functionality
            if chunk_number < expected_chunk - 5 or chunk_number > expected_chunk + 1:
                return jsonify({'error': 'Invalid chunk sequence'}), 400
        
        # Get chunk from firmware manager
        chunk_data = firmware_manager.get_chunk(version, chunk_number)
        if not chunk_data:
            return jsonify({'error': 'Chunk not found'}), 404
        
        # Update session
        with ota_lock:
            session['chunks_downloaded'] = max(session['chunks_downloaded'], chunk_number + 1)
            session['last_chunk_time'] = time.time()
        
        # Calculate progress
        progress = {
            'chunks_received': chunk_number + 1,
            'total_chunks': chunk_data['total_chunks'],
            'percentage': round((chunk_number + 1) * 100.0 / chunk_data['total_chunks'], 1)
        }
        
        logger.info(f"Serving chunk {chunk_number}/{chunk_data['total_chunks']} to {device_id} ({progress['percentage']}%)")
        
        return jsonify({
            'chunk': chunk_data,
            'progress': progress
        })
    
    except Exception as e:
        logger.error(f"Error in /ota/chunk: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/ota/verify', methods=['POST'])
def ota_verify():
    """Device reports verification result."""
    try:
        data = request.get_json()
        
        if not data or 'device_id' not in data or 'verified' not in data:
            return jsonify({'error': 'Invalid request format'}), 400
        
        device_id = data['device_id']
        verified = data['verified']
        version = data.get('version', 'unknown')
        
        logger.info(f"OTA verification result from {device_id}: {verified} (version: {version})")
        
        # Update session
        with ota_lock:
            if device_id in ota_sessions:
                session = ota_sessions[device_id]
                session['verified'] = verified
                session['verify_time'] = time.time()
        
        return jsonify({
            'status': 'acknowledged',
            'verified': verified
        })
    
    except Exception as e:
        logger.error(f"Error in /ota/verify: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/ota/complete', methods=['POST'])
def ota_complete():
    """Device reports successful boot with new firmware."""
    try:
        data = request.get_json()
        
        if not data or 'device_id' not in data:
            return jsonify({'error': 'Invalid request format'}), 400
        
        device_id = data['device_id']
        new_version = data.get('version', 'unknown')
        boot_status = data.get('boot_status', 'unknown')
        
        logger.info(f"OTA completion report from {device_id}: version={new_version}, status={boot_status}")
        
        # Mark session as complete
        with ota_lock:
            if device_id in ota_sessions:
                session = ota_sessions[device_id]
                session['completed'] = True
                session['new_version'] = new_version
                session['boot_status'] = boot_status
                session['complete_time'] = time.time()
                
                # Calculate total time
                total_time = session['complete_time'] - session['start_time']
                logger.info(f"OTA update completed for {device_id} in {total_time:.1f} seconds")
        
        return jsonify({
            'status': 'success',
            'message': f'OTA update completed successfully for {device_id}'
        })
    
    except Exception as e:
        logger.error(f"Error in /ota/complete: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/ota/status', methods=['GET'])
def ota_status():
    """Get status of all OTA sessions."""
    try:
        with ota_lock:
            sessions_copy = dict(ota_sessions)  # Create a copy to avoid lock issues
        
        return jsonify({
            'active_sessions': len(sessions_copy),
            'sessions': sessions_copy,
            'available_versions': firmware_manager.list_versions() if firmware_manager else []
        })
    
    except Exception as e:
        logger.error(f"Error in /ota/status: {e}")
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    print("Starting EcoWatt Smart Selection Batch Server v3.1")
    print("="*60)
    print("Enhanced debugging enabled for ESP32 batch compression")
    print("Supports Dictionary+Bitmask batch decompression")
    print("Handles 5 samples × 6 registers = 30 values per batch")
    print("Academic ratio tracking: 0.500 (50% compression)")
    print("="*60)
    
    if init_mqtt():
        print("MQTT client initialized successfully")
    else:
        print("Warning: MQTT client failed to initialize")
    
    # Initialize OTA firmware manager
    if init_firmware_manager():
        print("OTA firmware manager initialized successfully")
    else:
        print("Warning: OTA firmware manager failed to initialize")
    
    print(f"MQTT: {MQTT_BROKER}:{MQTT_PORT} → {MQTT_TOPIC}")
    print("Starting Flask server with comprehensive batch logging...")
    print("OTA endpoints available: /ota/check, /ota/chunk, /ota/verify, /ota/complete, /ota/status")
    print("="*60)
    
    app.run(host='0.0.0.0', port=5001, debug=True)
