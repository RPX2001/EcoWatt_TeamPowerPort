"""
MQTT utilities for EcoWatt Flask server
Handles MQTT client initialization, callbacks, and message handling
"""

import paho.mqtt.client as mqtt
import json
import time
import threading
import logging

logger = logging.getLogger(__name__)

# MQTT Configuration (should be imported from config.py)
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"
MQTT_SETTINGS_TOPIC = "esp32/ecowatt_settings"

# Global MQTT client state
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


def on_connect(client, userdata, flags, rc):
    """
    MQTT connection callback
    
    Args:
        client: MQTT client instance
        userdata: User data
        flags: Connection flags
        rc: Return code (0 = success)
    """
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        logger.info(f"Connected to MQTT broker at {MQTT_BROKER}")
        
        # Subscribe to settings topic on connection
        try:
            client.subscribe(MQTT_SETTINGS_TOPIC, qos=0)
            logger.info(f"Subscribed to topic '{MQTT_SETTINGS_TOPIC}'")
        except Exception as e:
            logger.warning(f"Failed to subscribe to '{MQTT_SETTINGS_TOPIC}': {e}")
    else:
        mqtt_connected = False
        logger.error(f"Failed to connect to MQTT broker. Return code: {rc}")


def on_publish(client, userdata, mid):
    """
    MQTT publish callback
    
    Args:
        client: MQTT client instance
        userdata: User data
        mid: Message ID
    """
    logger.info(f"Message {mid} published successfully")


def on_disconnect(client, userdata, rc):
    """
    MQTT disconnect callback
    
    Args:
        client: MQTT client instance
        userdata: User data
        rc: Return code
    """
    global mqtt_connected
    mqtt_connected = False
    if rc != 0:
        logger.warning(f"Unexpected disconnection from MQTT broker. Return code: {rc}")
    else:
        logger.info("Disconnected from MQTT broker")


def on_message(client, userdata, msg):
    """
    Handle incoming MQTT messages for settings topic
    
    Expected payload (JSON):
    {
        "Changed": true,
        "pollFreqChanged": true,
        "newPollTimer": 9,
        "uploadFreqChanged": false,
        "newUploadTimer": 11,
        "regsChanged": false,
        "regsCount": 10,
        "regs": 1023
    }
    
    Behavior:
    - Boolean flags are ORed with existing values
    - Timers replaced with newest values
    - Thread-safe with settings_lock
    
    Args:
        client: MQTT client instance
        userdata: User data
        msg: MQTT message
    """
    try:
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        logger.info(f"MQTT message received on {topic}: {payload}")

        # Check if message is for settings topic
        if topic.rstrip('/') != MQTT_SETTINGS_TOPIC and not topic.endswith('/ecowatt_settings'):
            logger.debug(f"Ignoring message from topic: {topic}")
            return

        data = json.loads(payload)

        with settings_lock:
            # OR boolean flags (accumulate changes)
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

            # Handle register configuration fields
            if 'regsChanged' in data:
                settings_state['regsChanged'] = bool(settings_state.get('regsChanged', False)) or bool(data.get('regsChanged', False))
            
            if 'regsCount' in data and isinstance(data.get('regsCount'), int):
                settings_state['regsCount'] = int(data.get('regsCount'))
            
            if 'regs' in data:
                # Store int16 value as-is
                settings_state['regs'] = data.get('regs')
                settings_state['regsChanged'] = True

            # Log concise update
            logger.info(f"Settings updated: Changed={settings_state.get('Changed', False)}, "
                       f"regsCount={settings_state.get('regsCount', 0)}")

    except json.JSONDecodeError as e:
        logger.error(f"Failed to parse MQTT message JSON: {e}")
    except Exception as e:
        logger.error(f"Error in on_message settings handler: {e}")


def init_mqtt(client_id=None):
    """
    Initialize MQTT client and connect to broker
    
    Args:
        client_id: Optional client ID (generated if not provided)
        
    Returns:
        bool: True if connection successful, False otherwise
    """
    global mqtt_client, mqtt_connected
    
    if client_id is None:
        client_id = f"flask_ecowatt_smart_server_{int(time.time())}"
    
    mqtt_client = mqtt.Client(client_id=client_id, clean_session=True)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_publish = on_publish
    mqtt_client.on_disconnect = on_disconnect
    mqtt_client.on_message = on_message
    
    try:
        logger.info(f"Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.loop_start()
        
        # Wait for connection
        time.sleep(2)
        
        if mqtt_connected:
            logger.info("MQTT initialization successful")
        else:
            logger.warning("MQTT client started but not yet connected")
        
        return mqtt_connected
        
    except Exception as e:
        logger.error(f"MQTT connection error: {e}")
        return False


def publish_mqtt(topic, payload, qos=0):
    """
    Publish message to MQTT broker
    
    Args:
        topic: MQTT topic
        payload: Message payload (string or dict)
        qos: Quality of Service (0, 1, or 2)
        
    Returns:
        bool: True if publish successful, False otherwise
    """
    global mqtt_client, mqtt_connected
    
    if mqtt_client is None:
        logger.error("MQTT client not initialized")
        return False
    
    if not mqtt_connected:
        logger.warning("MQTT not connected, attempting to publish anyway")
    
    try:
        # Convert dict to JSON string if needed
        if isinstance(payload, dict):
            payload = json.dumps(payload)
        
        result = mqtt_client.publish(topic, payload, qos=qos)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            logger.info(f"Published to {topic}: {payload[:100]}")
            return True
        else:
            logger.error(f"Failed to publish to {topic}: {result.rc}")
            return False
            
    except Exception as e:
        logger.error(f"Error publishing to MQTT: {e}")
        return False


def get_settings_state():
    """
    Get current settings state (thread-safe)
    
    Returns:
        dict: Copy of current settings state
    """
    with settings_lock:
        return settings_state.copy()


def update_settings_state(updates):
    """
    Update settings state (thread-safe)
    
    Args:
        updates: Dictionary of settings to update
        
    Returns:
        dict: Updated settings state
    """
    with settings_lock:
        settings_state.update(updates)
        return settings_state.copy()


def reset_settings_flags():
    """
    Reset all boolean flags in settings state (thread-safe)
    Used after ESP32 acknowledges changes
    
    Returns:
        dict: Updated settings state
    """
    with settings_lock:
        settings_state['Changed'] = False
        settings_state['pollFreqChanged'] = False
        settings_state['uploadFreqChanged'] = False
        settings_state['regsChanged'] = False
        return settings_state.copy()


def cleanup_mqtt():
    """
    Cleanup MQTT client resources
    """
    global mqtt_client, mqtt_connected
    
    if mqtt_client is not None:
        try:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
            logger.info("MQTT client disconnected")
        except Exception as e:
            logger.error(f"Error cleaning up MQTT: {e}")
        finally:
            mqtt_client = None
            mqtt_connected = False


def is_mqtt_connected():
    """
    Check if MQTT client is connected
    
    Returns:
        bool: True if connected, False otherwise
    """
    return mqtt_connected


# Export functions and variables
__all__ = [
    'init_mqtt',
    'publish_mqtt',
    'get_settings_state',
    'update_settings_state',
    'reset_settings_flags',
    'cleanup_mqtt',
    'is_mqtt_connected',
    'mqtt_client',
    'mqtt_connected',
    'settings_state',
    'settings_lock'
]
