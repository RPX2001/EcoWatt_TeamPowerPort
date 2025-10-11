# Flask Configuration
FLASK_HOST = '0.0.0.0'  # Listen on all interfaces
FLASK_PORT = 5001
FLASK_DEBUG = True

# MQTT Configuration
MQTT_BROKER = 'test.mosquitto.org'
MQTT_PORT = 1883
MQTT_TOPIC = 'esp32/processed_data'
MQTT_CLIENT_ID = 'flask_server_001'

# Processing Configuration
MULTIPLIER = 2