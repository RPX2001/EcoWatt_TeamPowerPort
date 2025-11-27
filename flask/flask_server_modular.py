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

# Import database
from database import Database

# Import logging utilities
from utils.logger_utils import init_logging, log_success

# Import all route blueprints
from routes import (
    general_bp,
    diagnostics_bp,
    aggregation_bp,
    security_bp,
    ota_bp,
    command_bp,
    fault_bp,
    power_bp
)
from routes.device_routes import device_bp, load_devices_from_database
from routes.config_routes import config_bp, load_configs_from_database
from routes.utilities_routes import utilities_bp

# Initialize Flask app
app = Flask(__name__)

# Configure maximum content length (16MB to accommodate large encrypted payloads)
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16 MB

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


def register_blueprints():
    """Register all route blueprints"""
    try:
        # Register blueprints with optional URL prefixes
        logger.debug("Registering general_bp")
        app.register_blueprint(general_bp)
        logger.debug("Registering diagnostics_bp")
        app.register_blueprint(diagnostics_bp)
        logger.debug("Registering aggregation_bp")
        app.register_blueprint(aggregation_bp)
        logger.debug("Registering security_bp")
        app.register_blueprint(security_bp)
        logger.debug("Registering ota_bp")
        app.register_blueprint(ota_bp)
        logger.debug("Registering command_bp")
        app.register_blueprint(command_bp)
        logger.debug("Registering fault_bp")
        app.register_blueprint(fault_bp)
        logger.debug("Registering device_bp")
        app.register_blueprint(device_bp)
        logger.debug("Registering config_bp")
        app.register_blueprint(config_bp)
        logger.debug("Registering utilities_bp")
        app.register_blueprint(utilities_bp)
        logger.debug("Registering power_bp")
        app.register_blueprint(power_bp)
        
        log_success(logger, "All route blueprints registered")
        
        # Debug: Print all registered routes
        logger.debug("Registered routes:")
        for rule in app.url_map.iter_rules():
            logger.debug(f"  {rule.methods} {rule.rule}")
        
        return True
        
    except Exception as e:
        logger.error(f"✗ Failed to register blueprints: {e}")
        logger.exception(e)
        return False


def print_startup_banner():
    """Print startup information"""
    print()
    print("╔════════════════════════════════════════════════════════════════════╗")
    print("║           EcoWatt Smart Server v2.0.0                              ║")
    print("║              Modular Architecture                                  ║")
    print("╚════════════════════════════════════════════════════════════════════╝")
    print()
    print("Features:")
    print("  ✓ Modular architecture (Routes → Handlers → Utilities)")
    print("  ✓ Compression (Dictionary, Temporal, Semantic, Bit-packed)")
    print("  ✓ Security validation with replay protection")
    print("  ✓ OTA firmware updates with chunking")
    print("  ✓ Command execution queue")
    print("  ✓ Fault injection testing")
    print("  ✓ Diagnostics tracking")
    print()
    print("Server: http://0.0.0.0:5001")
    print("────────────────────────────────────────────────────────────────────")
    print()


# Initialize database schema (must be before blueprints for imports)
Database.init_database()

# Load existing devices from database into memory cache
load_devices_from_database()

# Load existing device configurations from database into memory cache
load_configs_from_database()

# Register blueprints (must be outside __main__ for Flask reloader)
register_blueprints()


if __name__ == '__main__':
    # Print startup banner
    print_startup_banner()
    
    # Start Flask server
    log_success(logger, "Flask server starting on 0.0.0.0:5001")
    
    try:
        app.run(host='0.0.0.0', port=5001, debug=True)
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
        print()
        print("╔════════════════════════════════════════════════════════════════════╗")
        print("║                   Server Stopped Gracefully                        ║")
        print("╚════════════════════════════════════════════════════════════════════╝")
        print()
