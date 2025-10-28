#!/usr/bin/env python3
"""
EcoWatt Flask Server - Modular Architecture
Version: 2.0.0 (Refactored)

A modular Flask server for ESP32 EcoWatt devices with:
- Compression handling
- Security validation
- OTA firmware updates
- Command execution
- Fault injection testing
- Diagnostics tracking

Architecture:
    Routes → Handlers → Utilities
"""

from flask import Flask
from flask_cors import CORS
import logging
import time
import threading

# Import MQTT utilities
from utils.mqtt_utils import init_mqtt, mqtt_client, get_settings_state
from utils.logger_utils import init_logging

# Import all route blueprints
from routes import (
    general_bp,
    diagnostics_bp,
    aggregation_bp,
    security_bp,
    ota_bp,
    command_bp,
    fault_bp
)
from routes.device_routes import device_bp

# Initialize Flask app
app = Flask(__name__)

# Configure CORS for frontend development
CORS(app, resources={
    r"/*": {
        "origins": ["http://localhost:5173", "http://localhost:5174"],
        "methods": ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
        "allow_headers": ["Content-Type", "Authorization"],
        "supports_credentials": True
    }
})

# Configure logging
init_logging()
logger = logging.getLogger(__name__)

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"
MQTT_CLIENT_ID = f"flask_ecowatt_smart_server_{int(time.time())}"

# Settings state (thread-safe, managed by mqtt_utils)
settings_lock = threading.Lock()


# Track if blueprints have been registered
_blueprints_registered = False


def register_blueprints():
    """Register all route blueprints (only once)"""
    global _blueprints_registered
    
    if _blueprints_registered:
        logger.debug("Blueprints already registered, skipping")
        return True
    
    try:
        # Register blueprints with optional URL prefixes
        app.register_blueprint(general_bp)
        app.register_blueprint(diagnostics_bp)
        app.register_blueprint(aggregation_bp)
        app.register_blueprint(security_bp)
        app.register_blueprint(ota_bp)
        app.register_blueprint(command_bp)
        app.register_blueprint(fault_bp)
        app.register_blueprint(device_bp)
        
        _blueprints_registered = True
        logger.info("✓ All blueprints registered successfully")
        return True
        
    except Exception as e:
        logger.error(f"Failed to register blueprints: {e}")
        return False


# Register blueprints immediately when module is imported (for testing)
register_blueprints()


def print_startup_banner():
    """Print startup information"""
    print("=" * 70)
    print("EcoWatt Smart Server v2.0.0 - Modular Architecture")
    print("=" * 70)
    print("Features:")
    print("  ✓ Modular architecture (Routes → Handlers → Utilities)")
    print("  ✓ Compression handling (Dictionary, Temporal, Semantic, Bit-packed)")
    print("  ✓ Security validation with replay protection")
    print("  ✓ OTA firmware updates with chunking")
    print("  ✓ Command execution queue")
    print("  ✓ Fault injection testing")
    print("  ✓ Diagnostics tracking")
    print("=" * 70)
    print(f"MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"MQTT Topic: {MQTT_TOPIC}")
    print("=" * 70)
    print("Available Endpoints:")
    print("  General:     GET  /              - API information")
    print("               GET  /health        - Health check")
    print("               GET  /stats         - All statistics")
    print("               GET  /status        - System status")
    print("               GET  /ping          - Simple ping")
    print("")
    print("  Diagnostics: GET/POST/DELETE /diagnostics/<device_id>")
    print("               GET  /diagnostics/summary")
    print("")
    print("  Aggregation: POST /aggregated/<device_id>")
    print("               GET/DELETE /compression/stats")
    print("")
    print("  Security:    POST /security/validate/<device_id>")
    print("               GET/DELETE /security/stats")
    print("               DELETE /security/nonces")
    print("")
    print("  OTA:         GET  /ota/check/<device_id>")
    print("               POST /ota/initiate/<device_id>")
    print("               GET  /ota/chunk/<device_id>")
    print("               POST /ota/complete/<device_id>")
    print("               GET  /ota/status")
    print("")
    print("  Commands:    POST /commands/<device_id>")
    print("               GET  /commands/<device_id>/poll")
    print("               POST /commands/<device_id>/result")
    print("               GET  /commands/status/<command_id>")
    print("               GET/DELETE /commands/stats")
    print("")
    print("  Fault:       POST /fault/enable")
    print("               POST /fault/disable")
    print("               GET  /fault/status")
    print("               GET  /fault/available")
    print("               GET/DELETE /fault/stats")
    print("=" * 70)


if __name__ == '__main__':
    # Print startup banner
    print_startup_banner()
    
    # Initialize MQTT client
    mqtt_success = init_mqtt(
        broker=MQTT_BROKER,
        port=MQTT_PORT,
        client_id=MQTT_CLIENT_ID,
        data_topic=MQTT_TOPIC
    )
    
    if mqtt_success:
        logger.info("✓ MQTT client initialized successfully")
    else:
        logger.warning("⚠ MQTT client failed to initialize")
    
    # Register all blueprints
    if register_blueprints():
        logger.info("✓ All route blueprints registered")
    else:
        logger.error("✗ Failed to register blueprints")
        exit(1)
    
    # Start Flask server
    logger.info("Starting Flask server on 0.0.0.0:5001")
    print("=" * 70)
    print("Server starting... Press Ctrl+C to stop")
    print("=" * 70)
    
    try:
        app.run(host='0.0.0.0', port=5001, debug=True)
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
        print("\n" + "=" * 70)
        print("Server stopped gracefully")
        print("=" * 70)
