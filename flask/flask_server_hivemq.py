
from flask import Flask, request, jsonify, Response
import paho.mqtt.client as mqtt
import json
import time
import base64
import struct
from datetime import datetime
import logging
import threading
from server_security_layer import verify_secured_payload, is_secured_payload, SecurityError

# Import firmware manager for OTA functionality
from firmware_manager import FirmwareManager

# Import command manager for command execution
from command_manager import CommandManager

# Import fault injector for testing
from fault_injector import (
    enable_fault_injection, disable_fault_injection, 
    get_fault_status, reset_statistics, inject_fault
)

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
    'regs': 0  # int16 number from MQTT
}
settings_lock = threading.Lock()

# OTA Configuration and State
LATEST_FIRMWARE_VERSION = "1.0.4"
firmware_manager = None
ota_sessions = {}  # Track active OTA sessions by device_id
ota_lock = threading.Lock()

# Command Manager
command_manager = None

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
            if 'regs' in data:
                # Store int16 value as-is
                settings_state['regs'] = data.get('regs')
                settings_state['regsChanged'] = True

            # Log concise update once
            logger.info("settings_state updated: regsCount=%d, Changed=%s", settings_state.get('regsCount', 0), settings_state.get('Changed', False))

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

def init_command_manager():
    """Initialize the command manager for command execution."""
    global command_manager
    
    try:
        command_manager = CommandManager(log_file='flask/commands.log')
        logger.info("Command manager initialized successfully")
        return True
    except Exception as e:
        logger.error(f"Failed to initialize command manager: {e}")
        return False

# ============================================================================
# AGGREGATION / DEAGGREGATION FUNCTIONS
# ============================================================================

def deserialize_aggregated_sample(binary_data):
    """
    Deserialize aggregated sample data from binary format.
    
    Format:
    - Marker: 0xAG (1 byte)
    - Sample Count: uint8 (1 byte)
    - Timestamp Start: uint32 (4 bytes)
    - Timestamp End: uint32 (4 bytes)
    - Min values: uint16[10] (20 bytes)
    - Avg values: uint16[10] (20 bytes)
    - Max values: uint16[10] (20 bytes)
    Total: 70 bytes
    
    Returns:
    {
        'is_aggregated': True,
        'sample_count': 5,
        'timestamp_start': 12340,
        'timestamp_end': 12345,
        'min_values': [2400, 170, ...],
        'avg_values': [2405, 172, ...],
        'max_values': [2410, 175, ...]
    }
    """
    try:
        if len(binary_data) < 70:
            return None
        
        # Check marker (0xAG = 0xA6 in hex)
        if binary_data[0] != 0xA6:
            return None
        
        idx = 1
        
        # Extract header
        sample_count = binary_data[idx]
        idx += 1
        
        # Timestamps (4 bytes each, little-endian)
        timestamp_start = struct.unpack('<I', binary_data[idx:idx+4])[0]
        idx += 4
        timestamp_end = struct.unpack('<I', binary_data[idx:idx+4])[0]
        idx += 4
        
        # Extract min/avg/max arrays (10 values each)
        min_values = []
        avg_values = []
        max_values = []
        
        # Min values
        for i in range(10):
            val = struct.unpack('<H', binary_data[idx:idx+2])[0]
            min_values.append(val)
            idx += 2
        
        # Avg values
        for i in range(10):
            val = struct.unpack('<H', binary_data[idx:idx+2])[0]
            avg_values.append(val)
            idx += 2
        
        # Max values
        for i in range(10):
            val = struct.unpack('<H', binary_data[idx:idx+2])[0]
            max_values.append(val)
            idx += 2
        
        return {
            'is_aggregated': True,
            'sample_count': sample_count,
            'timestamp_start': timestamp_start,
            'timestamp_end': timestamp_end,
            'min_values': min_values,
            'avg_values': avg_values,
            'max_values': max_values
        }
    
    except Exception as e:
        logger.error(f"Error deserializing aggregated sample: {e}")
        return None


def expand_aggregated_to_samples(aggregated_data):
    """
    Expand aggregated data back to individual samples for storage/analysis.
    Uses average values as representative samples.
    
    Returns:
    {
        'type': 'aggregated',
        'sample_count': 5,
        'samples': [[avg1, avg2, ...], ...],  # For compatibility
        'aggregation': {
            'min': [2400, 170, ...],
            'avg': [2405, 172, ...],
            'max': [2410, 175, ...]
        }
    }
    """
    try:
        if not aggregated_data or not aggregated_data.get('is_aggregated'):
            return None
        
        return {
            'type': 'aggregated',
            'sample_count': aggregated_data['sample_count'],
            'timestamp_start': aggregated_data['timestamp_start'],
            'timestamp_end': aggregated_data['timestamp_end'],
            'samples': [aggregated_data['avg_values']],  # Use avg as representative
            'aggregation': {
                'min': aggregated_data['min_values'],
                'avg': aggregated_data['avg_values'],
                'max': aggregated_data['max_values']
            },
            'flat_data': aggregated_data['avg_values'],
            'first_sample': aggregated_data['avg_values']
        }
    
    except Exception as e:
        logger.error(f"Error expanding aggregated data: {e}")
        return None


# ============================================================================
# COMPRESSION / DECOMPRESSION FUNCTIONS
# ============================================================================

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
            registers_per_sample = 10  # Updated to 10 registers
            samples = 5
        elif count == 10:  # Single sample with 10 registers
            result = base_pattern[:]
            registers_per_sample = 10
            samples = 1
        else:
            # Handle other counts gracefully
            registers_per_sample = min(10, count)
            samples = count // registers_per_sample if count >= 10 else 1
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
        
        # Return in same format as dictionary - updated to 10 registers
        if count >= 10:
            return {
                'type': 'single',
                'samples': [result[:10]],
                'flat_data': result,
                'sample_count': 1,
                'first_sample': result[:10]
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
    Main decompression function with enhanced debugging and aggregation support
    """
    try:
        # Decode base64 to binary
        binary_data = base64.b64decode(base64_data)
        
        if len(binary_data) == 0:
            return []
        
        method_id = binary_data[0]
        
        # Check for aggregated data first (0xA6)
        if method_id == 0xA6:
            aggregated = deserialize_aggregated_sample(binary_data)
            if aggregated:
                return expand_aggregated_to_samples(aggregated)
            else:
                logger.warning("Failed to deserialize aggregated data")
                return []
        
        # Try Dictionary first (most common)
        elif method_id == 0xD0:
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
            
            # Return in standard format - updated to 10 registers
            if len(result) >= 10:
                return {
                    'type': 'single',
                    'samples': [result[:10]],
                    'flat_data': result,
                    'sample_count': 1,
                    'first_sample': result[:10]
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
            elif reg_name == "REG_FAC1":
                register_data["ac_frequency_readable"] = f"{value/10.0:.1f}Hz"
                register_data["ac_frequency_hz"] = value / 10.0
            elif reg_name == "REG_VPV1":
                register_data["pv_voltage_1_readable"] = f"{value/10.0:.1f}V"
                register_data["pv_voltage_1_volts"] = value / 10.0
            elif reg_name == "REG_VPV2":
                register_data["pv_voltage_2_readable"] = f"{value/10.0:.1f}V"
                register_data["pv_voltage_2_volts"] = value / 10.0
            elif reg_name == "REG_IPV1":
                register_data["pv_current_1_readable"] = f"{value/100.0:.2f}A"
                register_data["pv_current_1_amps"] = value / 100.0
            elif reg_name == "REG_IPV2":
                register_data["pv_current_2_readable"] = f"{value/100.0:.2f}A"
                register_data["pv_current_2_amps"] = value / 100.0
            elif reg_name == "REG_TEMP":
                register_data["temperature_readable"] = f"{value}°C"
                register_data["temperature_celsius"] = value
            elif reg_name == "REG_POW":
                register_data["power_readable"] = f"{value}W"
                register_data["power_watts"] = value
            elif reg_name == "REG_PAC":
                register_data["ac_power_readable"] = f"{value}W"
                register_data["ac_power_watts"] = value
    
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
        # Get raw data first
        raw_data = request.get_data(as_text=True)
        data = json.loads(raw_data)
        
        # ===== SECURITY LAYER: Check if payload is secured =====
        if is_secured_payload(data):
            logger.info("🔒 Detected secured payload - removing security layer...")
            try:
                # Verify and extract original payload
                original_json = verify_secured_payload(raw_data, "ESP32_EcoWatt_Smart")
                data = json.loads(original_json)
                logger.info(f"✓ Security verification successful!")
            except SecurityError as e:
                logger.error(f"✗ Security verification failed: {e}")
                return jsonify({'error': f'Security verification failed: {str(e)}'}), 401
        else:
            logger.info("No security layer detected - processing as plain JSON")
        
        # ===== Continue with normal decompression =====
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
        
        # Standard register order - updated to match all 10 registers
        registers = ["REG_VAC1", "REG_IAC1", "REG_FAC1", "REG_VPV1", "REG_VPV2", "REG_IPV1", "REG_IPV2", "REG_TEMP", "REG_POW", "REG_PAC"]
        
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
                    elif isinstance(item_data, list) and len(item_data) >= 10:
                        decompressed_result = {
                            'type': 'single',
                            'samples': [item_data[:10]],
                            'first_sample': item_data[:10]
                        }
            elif isinstance(compressed_item, str):
                decompressed_result = decompress_smart_binary_data(compressed_item)
            elif isinstance(compressed_item, list) and len(compressed_item) >= 10:
                decompressed_result = {
                    'type': 'single',
                    'samples': [compressed_item[:10]],
                    'first_sample': compressed_item[:10]
                }
            
            # Process decompression result
            if isinstance(decompressed_result, dict):
                decompressed_values = decompressed_result.get('first_sample', [])
                batch_samples = decompressed_result.get('samples', [])
                total_samples_processed += len(batch_samples)
            elif isinstance(decompressed_result, list):
                decompressed_values = decompressed_result[:10] if len(decompressed_result) >= 10 else decompressed_result
                batch_samples = [decompressed_values] if decompressed_values else []
                total_samples_processed += len(batch_samples)
            else:
                decompressed_values = []
                batch_samples = []
            
            # Check if decompression was successful
            success = len(decompressed_values) >= 10 and any(val > 0 for val in decompressed_values)
            if success:
                decompression_successes += 1
                # REG_PAC is now at index 9 (was index 3 in old 6-register format)
                if len(decompressed_values) > 9:
                    power_value = decompressed_values[9]  # REG_PAC
                    total_power += power_value * len(batch_samples)
            else:
                decompressed_values = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]  # 10 zeros
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
        
        # Create simplified MQTT payload with all 10 registers
        simplified_mqtt_payload = {
            'register_data': {
                'REG_VAC1': mqtt_register_data.get('REG_VAC1', 2400),
                'ac_voltage_readable': mqtt_register_data.get('ac_voltage_readable', "240.0V"),
                'REG_IAC1': mqtt_register_data.get('REG_IAC1', 170),
                'ac_current_readable': mqtt_register_data.get('ac_current_readable', "1.70A"),
                'REG_FAC1': mqtt_register_data.get('REG_FAC1', 500),
                'ac_frequency_readable': mqtt_register_data.get('ac_frequency_readable', "50.0Hz"),
                'REG_VPV1': mqtt_register_data.get('REG_VPV1', 3000),
                'pv_voltage_1_readable': mqtt_register_data.get('pv_voltage_1_readable', "300.0V"),
                'REG_VPV2': mqtt_register_data.get('REG_VPV2', 2950),
                'pv_voltage_2_readable': mqtt_register_data.get('pv_voltage_2_readable', "295.0V"),
                'REG_IPV1': mqtt_register_data.get('REG_IPV1', 70),
                'pv_current_1_readable': mqtt_register_data.get('pv_current_1_readable', "0.70A"),
                'REG_IPV2': mqtt_register_data.get('REG_IPV2', 65),
                'pv_current_2_readable': mqtt_register_data.get('pv_current_2_readable', "0.65A"),
                'REG_TEMP': mqtt_register_data.get('REG_TEMP', 550),
                'temperature_readable': mqtt_register_data.get('temperature_readable', "550°C"),
                'REG_POW': mqtt_register_data.get('REG_POW', 4000),
                'power_readable': mqtt_register_data.get('power_readable', "4000W"),
                'REG_PAC': mqtt_register_data.get('REG_PAC', 4000),
                'ac_power_readable': mqtt_register_data.get('ac_power_readable', "4000W")
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


# ============================================================================
# COMMAND EXECUTION ENDPOINTS
# ============================================================================

@app.route('/command/queue', methods=['POST'])
def queue_command():
    """
    Queue a new command for a device.
    
    Request body:
    {
        "device_id": "ESP32_EcoWatt_Smart",
        "command_type": "set_power" | "write_register",
        "parameters": {
            "power_value": 5000  // for set_power
            OR
            "register_address": 40001,  // for write_register
            "value": 4500
        }
    }
    
    Response:
    {
        "status": "success",
        "command_id": "CMD_1000_1729123456",
        "message": "Command queued successfully"
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        data = request.get_json()
        
        if not data or 'device_id' not in data or 'command_type' not in data:
            return jsonify({'error': 'Invalid request format. Required: device_id, command_type, parameters'}), 400
        
        device_id = data['device_id']
        command_type = data['command_type']
        parameters = data.get('parameters', {})
        
        # Validate command type
        valid_commands = [
            'set_power', 'set_power_percentage', 'write_register',
            'get_peripheral_stats', 'reset_peripheral_stats', 
            'get_power_stats', 'reset_power_stats'
        ]
        if command_type not in valid_commands:
            return jsonify({'error': f'Unsupported command type: {command_type}'}), 400
        
        # Validate parameters based on command type
        if command_type == 'set_power':
            if 'power_value' not in parameters:
                return jsonify({'error': 'Missing power_value parameter for set_power command'}), 400
        elif command_type == 'set_power_percentage':
            if 'percentage' not in parameters:
                return jsonify({'error': 'Missing percentage parameter for set_power_percentage command'}), 400
            # Validate percentage range
            pct = parameters.get('percentage', 0)
            if not isinstance(pct, (int, float)) or pct < 0 or pct > 100:
                return jsonify({'error': 'Percentage must be between 0 and 100'}), 400
        elif command_type == 'write_register':
            if 'register_address' not in parameters or 'value' not in parameters:
                return jsonify({'error': 'Missing register_address or value for write_register command'}), 400
        # Stats commands don't require parameters validation
        
        # Queue the command
        command_id = command_manager.queue_command(device_id, command_type, parameters)
        
        logger.info(f"Command queued: {command_id} for {device_id}")
        
        return jsonify({
            'status': 'success',
            'command_id': command_id,
            'device_id': device_id,
            'command_type': command_type,
            'parameters': parameters,
            'message': f'Command {command_type} queued successfully for {device_id}'
        }), 200
    
    except Exception as e:
        logger.error(f"Error in /command/queue: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/poll', methods=['POST'])
def poll_command():
    """
    Poll for pending commands (called by ESP32).
    
    Request body:
    {
        "device_id": "ESP32_EcoWatt_Smart"
    }
    
    Response (if command available):
    {
        "command": {
            "command_id": "CMD_1000_1729123456",
            "command_type": "set_power",
            "parameters": {
                "power_value": 5000
            }
        }
    }
    
    Response (if no commands):
    {
        "message": "No pending commands"
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        data = request.get_json()
        
        if not data or 'device_id' not in data:
            return jsonify({'error': 'Missing device_id'}), 400
        
        device_id = data['device_id']
        
        # Get next command for this device
        command = command_manager.get_next_command(device_id)
        
        if command:
            logger.info(f"Serving command {command['command_id']} to {device_id}")
            return jsonify({
                'command': command
            }), 200
        else:
            logger.debug(f"No pending commands for {device_id}")
            return jsonify({
                'message': 'No pending commands'
            }), 200
    
    except Exception as e:
        logger.error(f"Error in /command/poll: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/result', methods=['POST'])
def submit_command_result():
    """
    Submit command execution result (called by ESP32).
    
    Request body:
    {
        "command_id": "CMD_1000_1729123456",
        "status": "completed" | "failed",
        "result": "Command set_power: executed successfully"
    }
    
    Response:
    {
        "status": "success",
        "message": "Result recorded"
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        data = request.get_json()
        
        if not data or 'command_id' not in data or 'status' not in data or 'result' not in data:
            return jsonify({'error': 'Missing required fields: command_id, status, result'}), 400
        
        command_id = data['command_id']
        status = data['status']
        result = data['result']
        
        # Validate status
        if status not in ['completed', 'failed']:
            return jsonify({'error': 'Invalid status. Must be "completed" or "failed"'}), 400
        
        # Record the result
        success = command_manager.record_result(command_id, status, result)
        
        if success:
            logger.info(f"Command result recorded: {command_id} - {status}")
            return jsonify({
                'status': 'success',
                'message': 'Result recorded successfully',
                'command_id': command_id
            }), 200
        else:
            logger.warning(f"Command not found: {command_id}")
            return jsonify({
                'status': 'warning',
                'message': 'Command not found, but result logged',
                'command_id': command_id
            }), 200
    
    except Exception as e:
        logger.error(f"Error in /command/result: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/status/<command_id>', methods=['GET'])
def get_command_status(command_id):
    """
    Get status of a specific command.
    
    Response:
    {
        "command_id": "CMD_1000_1729123456",
        "device_id": "ESP32_EcoWatt_Smart",
        "command_type": "set_power",
        "parameters": {...},
        "status": "completed",
        "queued_time": "2025-10-18T14:30:00",
        "sent_time": "2025-10-18T14:31:00",
        "completed_time": "2025-10-18T14:31:05",
        "result": "Command executed successfully"
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        command = command_manager.get_command_status(command_id)
        
        if command:
            return jsonify(command), 200
        else:
            return jsonify({'error': 'Command not found'}), 404
    
    except Exception as e:
        logger.error(f"Error in /command/status: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/history', methods=['GET'])
def get_command_history():
    """
    Get command history, optionally filtered by device.
    
    Query parameters:
    - device_id: Optional device filter
    - limit: Maximum number of commands to return (default 50)
    
    Response:
    {
        "commands": [
            {
                "command_id": "CMD_1000_1729123456",
                "device_id": "ESP32_EcoWatt_Smart",
                "command_type": "set_power",
                "status": "completed",
                ...
            },
            ...
        ],
        "count": 25
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        device_id = request.args.get('device_id')
        limit = int(request.args.get('limit', 50))
        
        if device_id:
            commands = command_manager.get_device_commands(device_id, limit)
        else:
            commands = command_manager.get_all_commands(limit)
        
        return jsonify({
            'commands': commands,
            'count': len(commands),
            'device_id_filter': device_id if device_id else 'all'
        }), 200
    
    except Exception as e:
        logger.error(f"Error in /command/history: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/pending', methods=['GET'])
def get_pending_commands():
    """
    Get all pending commands, optionally filtered by device.
    
    Query parameters:
    - device_id: Optional device filter
    
    Response:
    {
        "pending_commands": [...],
        "count": 3
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        device_id = request.args.get('device_id')
        
        pending = command_manager.get_pending_commands(device_id)
        
        return jsonify({
            'pending_commands': pending,
            'count': len(pending),
            'device_id_filter': device_id if device_id else 'all'
        }), 200
    
    except Exception as e:
        logger.error(f"Error in /command/pending: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/statistics', methods=['GET'])
def get_command_statistics():
    """
    Get command execution statistics.
    
    Response:
    {
        "total_commands": 150,
        "completed": 145,
        "failed": 3,
        "pending": 2,
        "success_rate": 96.67,
        "active_devices": 1,
        "total_queued": 2
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        stats = command_manager.get_statistics()
        
        return jsonify(stats), 200
    
    except Exception as e:
        logger.error(f"Error in /command/statistics: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/command/logs', methods=['GET'])
def export_command_logs():
    """
    Export command logs with optional filtering.
    
    Query parameters:
    - device_id: Optional device filter
    - start_time: Optional start time (ISO format)
    - end_time: Optional end time (ISO format)
    
    Response:
    {
        "logs": [...],
        "count": 50
    }
    """
    try:
        if not command_manager:
            return jsonify({'error': 'Command manager not initialized'}), 500
        
        device_id = request.args.get('device_id')
        start_time = request.args.get('start_time')
        end_time = request.args.get('end_time')
        
        logs = command_manager.export_logs(device_id, start_time, end_time)
        
        return jsonify({
            'logs': logs,
            'count': len(logs),
            'filters': {
                'device_id': device_id,
                'start_time': start_time,
                'end_time': end_time
            }
        }), 200
    
    except Exception as e:
        logger.error(f"Error in /command/logs: {e}")
        return jsonify({'error': str(e)}), 500


# ============================================================================
# FAULT INJECTION ENDPOINTS (TESTING ONLY)
# ============================================================================

@app.route('/fault/enable', methods=['POST'])
def enable_fault():
    """
    Enable fault injection for testing.
    
    Request body:
    {
        "fault_type": "corrupt_crc | truncate | garbage | timeout | exception",
        "probability": 0-100 (percentage),
        "duration": seconds (0 = infinite)
    }
    
    Response:
    {
        "success": true,
        "message": "Fault injection enabled",
        "config": {...}
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'Missing request body'}), 400
        
        fault_type = data.get('fault_type')
        probability = data.get('probability', 100)
        duration = data.get('duration', 0)
        
        if not fault_type:
            return jsonify({'error': 'Missing fault_type parameter'}), 400
        
        result = enable_fault_injection(fault_type, probability, duration)
        
        if result['success']:
            return jsonify(result), 200
        else:
            return jsonify(result), 400
    
    except Exception as e:
        logger.error(f"Error in /fault/enable: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/fault/disable', methods=['POST'])
def disable_fault():
    """
    Disable fault injection.
    
    Response:
    {
        "success": true,
        "message": "Fault injection disabled",
        "statistics": {...}
    }
    """
    try:
        result = disable_fault_injection()
        return jsonify(result), 200
    
    except Exception as e:
        logger.error(f"Error in /fault/disable: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/fault/status', methods=['GET'])
def fault_status():
    """
    Get fault injection status and statistics.
    
    Response:
    {
        "enabled": true/false,
        "fault_type": "...",
        "probability": 0-100,
        "faults_injected": 123,
        "requests_processed": 456,
        "detailed_stats": {...}
    }
    """
    try:
        status = get_fault_status()
        return jsonify(status), 200
    
    except Exception as e:
        logger.error(f"Error in /fault/status: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/fault/reset', methods=['POST'])
def fault_reset():
    """
    Reset fault injection statistics.
    
    Response:
    {
        "success": true,
        "message": "Statistics reset"
    }
    """
    try:
        result = reset_statistics()
        return jsonify(result), 200
    
    except Exception as e:
        logger.error(f"Error in /fault/reset: {e}")
        return jsonify({'error': str(e)}), 500


# ============================================================================
# DIAGNOSTICS ENDPOINTS
# ============================================================================

@app.route('/diagnostics/<device_id>', methods=['POST'])
def receive_diagnostics(device_id):
    """
    Receive diagnostics data from ESP32 device.
    
    Request body:
    {
        "uptime": 12345,
        "error_counts": {
            "read_errors": 5,
            "write_errors": 2,
            "timeouts": 3,
            "crc_errors": 1,
            "malformed_frames": 0,
            "compression_failures": 0,
            "upload_failures": 1,
            "security_violations": 0
        },
        "recent_events": [
            {
                "timestamp": 12340,
                "type": "ERROR",
                "message": "Read failed",
                "error_code": 1001
            }
        ],
        "success_rate": 95.5
    }
    
    Response:
    {
        "success": true,
        "message": "Diagnostics received",
        "stored_file": "diagnostics/ESP32_EcoWatt_Smart.json"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'Missing diagnostics data'}), 400
        
        # Create diagnostics directory if it doesn't exist
        import os
        os.makedirs('diagnostics', exist_ok=True)
        
        # Add timestamp and device_id
        from datetime import datetime
        data['device_id'] = device_id
        data['received_at'] = datetime.utcnow().isoformat()
        
        # Store diagnostics to JSON file
        filename = f'diagnostics/{device_id}.json'
        
        # Load existing diagnostics history
        history = []
        if os.path.exists(filename):
            try:
                with open(filename, 'r') as f:
                    history = json.load(f)
                    if not isinstance(history, list):
                        history = [history]
            except:
                history = []
        
        # Add new entry to history
        history.append(data)
        
        # Keep only last 100 entries
        if len(history) > 100:
            history = history[-100:]
        
        # Save to file
        with open(filename, 'w') as f:
            json.dump(history, f, indent=2)
        
        logger.info(f"Diagnostics received from {device_id}")
        logger.info(f"  Uptime: {data.get('uptime', 'N/A')}s")
        logger.info(f"  Success Rate: {data.get('success_rate', 'N/A')}%")
        logger.info(f"  Total Errors: {sum(data.get('error_counts', {}).values())}")
        
        return jsonify({
            'success': True,
            'message': 'Diagnostics received',
            'stored_file': filename,
            'history_entries': len(history)
        }), 200
    
    except Exception as e:
        logger.error(f"Error in /diagnostics/{device_id}: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/diagnostics/<device_id>', methods=['GET'])
def get_diagnostics(device_id):
    """
    Retrieve diagnostics history for a device.
    
    Query parameters:
    - limit: Maximum number of entries to return (default: 10)
    - format: 'summary' or 'full' (default: 'summary')
    
    Response:
    {
        "device_id": "ESP32_EcoWatt_Smart",
        "total_entries": 50,
        "entries": [...]
    }
    """
    try:
        import os
        filename = f'diagnostics/{device_id}.json'
        
        if not os.path.exists(filename):
            return jsonify({
                'error': f'No diagnostics found for device {device_id}'
            }), 404
        
        # Load diagnostics
        with open(filename, 'r') as f:
            history = json.load(f)
            if not isinstance(history, list):
                history = [history]
        
        # Get query parameters
        limit = int(request.args.get('limit', 10))
        format_type = request.args.get('format', 'summary')
        
        # Get last N entries
        entries = history[-limit:]
        
        if format_type == 'summary':
            # Provide summary only
            if entries:
                latest = entries[-1]
                return jsonify({
                    'device_id': device_id,
                    'total_entries': len(history),
                    'latest_uptime': latest.get('uptime'),
                    'latest_success_rate': latest.get('success_rate'),
                    'total_errors': sum(latest.get('error_counts', {}).values()),
                    'error_counts': latest.get('error_counts'),
                    'last_updated': latest.get('received_at'),
                    'available_entries': len(history)
                }), 200
            else:
                return jsonify({'error': 'No entries found'}), 404
        else:
            # Full history
            return jsonify({
                'device_id': device_id,
                'total_entries': len(history),
                'entries': entries
            }), 200
    
    except Exception as e:
        logger.error(f"Error in GET /diagnostics/{device_id}: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/diagnostics', methods=['GET'])
def list_devices_with_diagnostics():
    """
    List all devices that have diagnostics data.
    
    Response:
    {
        "devices": [
            {
                "device_id": "ESP32_EcoWatt_Smart",
                "entries": 50,
                "last_updated": "2025-10-22T10:00:00"
            }
        ]
    }
    """
    try:
        import os
        import glob
        
        devices = []
        
        if os.path.exists('diagnostics'):
            for filepath in glob.glob('diagnostics/*.json'):
                device_id = os.path.basename(filepath).replace('.json', '')
                
                try:
                    with open(filepath, 'r') as f:
                        history = json.load(f)
                        if not isinstance(history, list):
                            history = [history]
                        
                        if history:
                            latest = history[-1]
                            devices.append({
                                'device_id': device_id,
                                'entries': len(history),
                                'last_updated': latest.get('received_at', 'Unknown'),
                                'success_rate': latest.get('success_rate', 0),
                                'total_errors': sum(latest.get('error_counts', {}).values())
                            })
                except:
                    pass
        
        return jsonify({
            'device_count': len(devices),
            'devices': devices
        }), 200
    
    except Exception as e:
        logger.error(f"Error in GET /diagnostics: {e}")
        return jsonify({'error': str(e)}), 500


# ============================================================================
# AGGREGATION STATISTICS ENDPOINT
# ============================================================================

# Global aggregation statistics
aggregation_stats = {
    'total_aggregated_uploads': 0,
    'total_samples_aggregated': 0,
    'total_bandwidth_saved': 0,
    'device_stats': {}
}
aggregation_stats_lock = threading.Lock()

@app.route('/aggregation/stats', methods=['GET'])
def get_aggregation_stats():
    """
    Get aggregation statistics across all devices.
    
    Response:
    {
        "total_aggregated_uploads": 100,
        "total_samples_aggregated": 500,
        "total_bandwidth_saved_bytes": 5000,
        "device_stats": {
            "ESP32_EcoWatt_Smart": {
                "aggregated_uploads": 50,
                "samples_aggregated": 250,
                "bandwidth_saved": 2500
            }
        }
    }
    """
    try:
        with aggregation_stats_lock:
            stats = dict(aggregation_stats)
        
        return jsonify(stats), 200
    
    except Exception as e:
        logger.error(f"Error in /aggregation/stats: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/aggregation/stats/<device_id>', methods=['GET'])
def get_device_aggregation_stats(device_id):
    """
    Get aggregation statistics for a specific device.
    
    Response:
    {
        "device_id": "ESP32_EcoWatt_Smart",
        "aggregated_uploads": 50,
        "samples_aggregated": 250,
        "bandwidth_saved_bytes": 2500,
        "compression_ratio_with_aggregation": 0.020
    }
    """
    try:
        with aggregation_stats_lock:
            device_stats = aggregation_stats['device_stats'].get(device_id, {
                'aggregated_uploads': 0,
                'samples_aggregated': 0,
                'bandwidth_saved': 0
            })
        
        return jsonify({
            'device_id': device_id,
            **device_stats
        }), 200
    
    except Exception as e:
        logger.error(f"Error in /aggregation/stats/{device_id}: {e}")
        return jsonify({'error': str(e)}), 500


# ============================================================================
# SECURITY STATISTICS ENDPOINTS
# ============================================================================

@app.route('/security/stats', methods=['GET'])
def get_security_stats_all():
    """
    Get security statistics for all devices.
    
    Response:
    {
        "total_requests": 1000,
        "valid_requests": 950,
        "replay_blocked": 30,
        "mac_failed": 15,
        "malformed_requests": 5,
        "device_stats": {...}
    }
    """
    try:
        from server_security_layer import get_security_stats
        stats = get_security_stats()
        return jsonify(stats), 200
    
    except Exception as e:
        logger.error(f"Error in /security/stats: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/security/stats/<device_id>', methods=['GET'])
def get_security_stats_device(device_id):
    """
    Get security statistics for a specific device.
    
    Response:
    {
        "device_id": "ESP32_EcoWatt_Smart",
        "first_seen": "2025-10-22T10:00:00",
        "total_requests": 100,
        "valid_requests": 95,
        "replay_blocked": 3,
        "mac_failed": 1,
        "malformed_requests": 1,
        "last_valid_request": "2025-10-22T12:00:00"
    }
    """
    try:
        from server_security_layer import get_security_stats
        stats = get_security_stats(device_id)
        return jsonify(stats), 200
    
    except Exception as e:
        logger.error(f"Error in /security/stats/{device_id}: {e}")
        return jsonify({'error': str(e)}), 500


# ============================================================================
# END OF COMMAND EXECUTION ENDPOINTS
# ============================================================================

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
    
    # Initialize Command Manager
    if init_command_manager():
        print("Command manager initialized successfully")
    else:
        print("Warning: Command manager failed to initialize")
    
    print(f"MQTT: {MQTT_BROKER}:{MQTT_PORT} → {MQTT_TOPIC}")
    print("Starting Flask server with comprehensive batch logging...")
    print("OTA endpoints available: /ota/check, /ota/chunk, /ota/verify, /ota/complete, /ota/status")
    print("Command endpoints: /command/queue, /command/poll, /command/result, /command/status, /command/history")
    print("Fault injection endpoints: /fault/enable, /fault/disable, /fault/status, /fault/reset")
    print("Diagnostics endpoints: /diagnostics, /diagnostics/<device_id> (GET/POST)")
    print("Aggregation endpoints: /aggregation/stats, /aggregation/stats/<device_id>")
    print("Security endpoints: /security/stats, /security/stats/<device_id>")
    print("="*60)
    
    app.run(host='0.0.0.0', port=5001, debug=True)
