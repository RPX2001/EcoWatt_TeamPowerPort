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
from utils.logger_utils import init_logging

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
from routes.device_routes import device_bp
from routes.config_routes import config_bp
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
        logger.info("Registering general_bp...")
        app.register_blueprint(general_bp)
        logger.info("Registering diagnostics_bp...")
        app.register_blueprint(diagnostics_bp)
        logger.info("Registering aggregation_bp...")
        app.register_blueprint(aggregation_bp)
        logger.info("Registering security_bp...")
        app.register_blueprint(security_bp)
        logger.info("Registering ota_bp...")
        app.register_blueprint(ota_bp)
        logger.info("Registering command_bp...")
        app.register_blueprint(command_bp)
        logger.info("Registering fault_bp...")
        app.register_blueprint(fault_bp)
        logger.info("Registering device_bp...")
        app.register_blueprint(device_bp)
        logger.info("Registering config_bp...")
        app.register_blueprint(config_bp)
        logger.info("Registering utilities_bp...")
        app.register_blueprint(utilities_bp)
        logger.info("Registering power_bp...")
        app.register_blueprint(power_bp)
        
        # Debug: Print all registered routes
        logger.info("✓ All route blueprints registered")
        logger.info("Registered routes:")
        for rule in app.url_map.iter_rules():
            logger.info(f"  {rule.methods} {rule.rule}")
        
        return True
        
    except Exception as e:
        logger.error(f"Failed to register blueprints: {e}")
        logger.exception(e)
        return False


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


# Initialize database schema (must be before blueprints for imports)
Database.init_database()

# Register blueprints (must be outside __main__ for Flask reloader)
register_blueprints()


if __name__ == '__main__':
    # Print startup banner
    print_startup_banner()
    
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
