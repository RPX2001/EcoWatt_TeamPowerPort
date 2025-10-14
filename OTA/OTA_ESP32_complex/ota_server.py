#!/usr/bin/env python3
"""
Secure OTA Firmware Server for ESP32 Devices

This Flask server provides secure over-the-air firmware updates for ESP32 devices.
It implements comprehensive security measures including:

- API key authentication for all endpoints
- HMAC-SHA256 firmware signing for authenticity verification
- HTTPS with self-signed certificates for encrypted communication
- Firmware version management and manifest serving
- Device update status tracking and reporting
- Comprehensive logging and monitoring

Security Features:
- Pre-shared API key validation
- HMAC signing of all firmware binaries
- SHA256 hash generation for integrity verification
- Request rate limiting to prevent abuse
- Secure file serving with path validation
- Comprehensive audit logging

@author ESP32 OTA Team
@version 1.0.0
@date 2024
"""

import os
import hashlib
import hmac
import json
import logging
import ssl
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from flask import Flask, request, jsonify, send_file, abort
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
import werkzeug.utils

# ========== Configuration ==========
class Config:
    """Server configuration class with security and operational settings"""
    
    # Server settings
    HOST = '0.0.0.0'
    PORT = 5000
    DEBUG = False
    
    # Security settings
    API_KEY = 'your-secure-api-key-here-change-this-in-production'
    HMAC_KEY = b'this-is-a-32-byte-hmac-key-change'  # Must be 32 bytes
    
    # SSL/TLS settings
    SSL_CERT_FILE = 'server.crt'
    SSL_KEY_FILE = 'server.key'
    USE_SSL = True
    
    # Firmware storage settings
    FIRMWARE_DIR = 'firmware'
    UPLOAD_DIR = 'uploads'
    MAX_FIRMWARE_SIZE = 2 * 1024 * 1024  # 2MB max firmware size
    
    # Rate limiting settings
    RATE_LIMIT_REQUESTS = "100 per hour"
    RATE_LIMIT_BURST = "10 per minute"
    
    # Logging settings
    LOG_LEVEL = logging.INFO
    LOG_FILE = 'ota_server.log'
    
    @classmethod
    def validate(cls):
        """Validate configuration settings"""
        if len(cls.HMAC_KEY) != 32:
            raise ValueError("HMAC_KEY must be exactly 32 bytes")
        
        if cls.API_KEY == 'your-secure-api-key-here-change-this-in-production':
            print("WARNING: Using default API key! Change this in production!")
        
        # Create required directories
        Path(cls.FIRMWARE_DIR).mkdir(exist_ok=True)
        Path(cls.UPLOAD_DIR).mkdir(exist_ok=True)

# Initialize Flask application
app = Flask(__name__)
app.config.from_object(Config)

# Validate configuration
Config.validate()

# Initialize rate limiter
limiter = Limiter(
    app,
    key_func=get_remote_address,
    default_limits=[Config.RATE_LIMIT_REQUESTS]
)

# ========== Logging Setup ==========
def setup_logging():
    """Configure comprehensive logging for the OTA server"""
    
    # Create formatter
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    
    # Setup file handler
    file_handler = logging.FileHandler(Config.LOG_FILE)
    file_handler.setLevel(Config.LOG_LEVEL)
    file_handler.setFormatter(formatter)
    
    # Setup console handler
    console_handler = logging.StreamHandler()
    console_handler.setLevel(Config.LOG_LEVEL)
    console_handler.setFormatter(formatter)
    
    # Configure app logger
    app.logger.setLevel(Config.LOG_LEVEL)
    app.logger.addHandler(file_handler)
    app.logger.addHandler(console_handler)
    
    # Configure werkzeug logger
    werkzeug_logger = logging.getLogger('werkzeug')
    werkzeug_logger.setLevel(logging.WARNING)

setup_logging()

# ========== Security Functions ==========
def validate_api_key(request) -> bool:
    """
    Validate API key from request headers
    
    Args:
        request: Flask request object
        
    Returns:
        bool: True if API key is valid, False otherwise
    """
    api_key = request.headers.get('X-API-Key')
    if not api_key:
        app.logger.warning(f"Missing API key from {request.remote_addr}")
        return False
    
    if api_key != Config.API_KEY:
        app.logger.warning(f"Invalid API key from {request.remote_addr}: {api_key}")
        return False
    
    return True

def calculate_file_hash(filepath: str) -> str:
    """
    Calculate SHA256 hash of a file
    
    Args:
        filepath: Path to the file
        
    Returns:
        str: Hexadecimal SHA256 hash
    """
    sha256_hash = hashlib.sha256()
    try:
        with open(filepath, "rb") as f:
            # Read file in chunks to handle large files efficiently
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()
    except Exception as e:
        app.logger.error(f"Error calculating hash for {filepath}: {e}")
        raise

def calculate_hmac_signature(filepath: str) -> str:
    """
    Calculate HMAC-SHA256 signature of a file
    
    Args:
        filepath: Path to the file
        
    Returns:
        str: Hexadecimal HMAC signature
    """
    try:
        with open(filepath, "rb") as f:
            file_data = f.read()
        
        signature = hmac.new(
            Config.HMAC_KEY,
            file_data,
            hashlib.sha256
        ).hexdigest()
        
        return signature
    except Exception as e:
        app.logger.error(f"Error calculating HMAC for {filepath}: {e}")
        raise

def secure_filename_validation(filename: str) -> bool:
    """
    Validate filename for security
    
    Args:
        filename: Filename to validate
        
    Returns:
        bool: True if filename is safe, False otherwise
    """
    # Use werkzeug's secure_filename and additional checks
    secure_name = werkzeug.utils.secure_filename(filename)
    if secure_name != filename:
        return False
    
    # Additional security checks
    dangerous_patterns = ['..', '/', '\\', '~', '$', '`', '|', ';', '&']
    for pattern in dangerous_patterns:
        if pattern in filename:
            return False
    
    return True

# ========== Firmware Management ==========
class FirmwareManager:
    """Manages firmware versions, files, and metadata"""
    
    def __init__(self):
        self.firmware_dir = Path(Config.FIRMWARE_DIR)
        self.metadata_file = self.firmware_dir / 'manifest.json'
        self.load_manifest()
    
    def load_manifest(self):
        """Load firmware manifest from disk"""
        try:
            if self.metadata_file.exists():
                with open(self.metadata_file, 'r') as f:
                    self.manifest = json.load(f)
                app.logger.info("Loaded existing manifest")
            else:
                self.manifest = {
                    "current_version": "1.0.0",
                    "versions": {},
                    "force_update": False
                }
                self.save_manifest()
                app.logger.info("Created new manifest")
        except Exception as e:
            app.logger.error(f"Error loading manifest: {e}")
            self.manifest = {
                "current_version": "1.0.0",
                "versions": {},
                "force_update": False
            }
    
    def save_manifest(self):
        """Save firmware manifest to disk"""
        try:
            with open(self.metadata_file, 'w') as f:
                json.dump(self.manifest, f, indent=2)
            app.logger.info("Manifest saved successfully")
        except Exception as e:
            app.logger.error(f"Error saving manifest: {e}")
    
    def add_firmware(self, version: str, filepath: str) -> Dict:
        """
        Add new firmware version to manifest
        
        Args:
            version: Firmware version string
            filepath: Path to firmware binary file
            
        Returns:
            Dict: Firmware metadata
        """
        try:
            # Calculate file hashes
            sha256_hash = calculate_file_hash(filepath)
            hmac_signature = calculate_hmac_signature(filepath)
            file_size = os.path.getsize(filepath)
            
            # Create firmware metadata
            firmware_info = {
                "version": version,
                "filename": os.path.basename(filepath),
                "sha256": sha256_hash,
                "hmac": hmac_signature,
                "size": file_size,
                "upload_date": datetime.now().isoformat(),
                "download_url": f"/firmware/{version}.bin"
            }
            
            # Update manifest
            self.manifest["versions"][version] = firmware_info
            self.manifest["current_version"] = version
            self.save_manifest()
            
            app.logger.info(f"Added firmware version {version}")
            return firmware_info
            
        except Exception as e:
            app.logger.error(f"Error adding firmware {version}: {e}")
            raise
    
    def get_latest_firmware(self) -> Dict:
        """
        Get latest firmware information
        
        Returns:
            Dict: Latest firmware metadata
        """
        current_version = self.manifest.get("current_version")
        if not current_version or current_version not in self.manifest["versions"]:
            return None
        
        firmware_info = self.manifest["versions"][current_version].copy()
        firmware_info["force_update"] = self.manifest.get("force_update", False)
        
        return firmware_info
    
    def get_firmware_path(self, version: str) -> Optional[str]:
        """
        Get filesystem path for firmware version
        
        Args:
            version: Firmware version string
            
        Returns:
            str: Full path to firmware file, None if not found
        """
        if version not in self.manifest["versions"]:
            return None
        
        filename = self.manifest["versions"][version]["filename"]
        filepath = self.firmware_dir / filename
        
        if filepath.exists():
            return str(filepath)
        
        return None

# Initialize firmware manager
firmware_manager = FirmwareManager()

# ========== Device Status Tracking ==========
class DeviceStatusTracker:
    """Tracks device update status and statistics"""
    
    def __init__(self):
        self.device_status = {}
        self.update_history = []
    
    def update_device_status(self, device_id: str, status_data: Dict):
        """
        Update device status information
        
        Args:
            device_id: Unique device identifier
            status_data: Status information dictionary
        """
        status_data["last_seen"] = datetime.now().isoformat()
        self.device_status[device_id] = status_data
        
        # Add to history
        history_entry = status_data.copy()
        history_entry["device_id"] = device_id
        self.update_history.append(history_entry)
        
        # Keep only last 1000 history entries
        if len(self.update_history) > 1000:
            self.update_history = self.update_history[-1000:]
        
        app.logger.info(f"Updated status for device {device_id}: {status_data.get('status', 'unknown')}")
    
    def get_device_status(self, device_id: str) -> Optional[Dict]:
        """
        Get current status for a device
        
        Args:
            device_id: Unique device identifier
            
        Returns:
            Dict: Device status information, None if not found
        """
        return self.device_status.get(device_id)
    
    def get_all_devices(self) -> Dict:
        """Get status of all tracked devices"""
        return self.device_status.copy()
    
    def get_update_history(self, limit: int = 100) -> List[Dict]:
        """
        Get recent update history
        
        Args:
            limit: Maximum number of history entries to return
            
        Returns:
            List[Dict]: Recent update history entries
        """
        return self.update_history[-limit:]

# Initialize device status tracker
device_tracker = DeviceStatusTracker()

# ========== API Endpoints ==========

@app.before_request
def before_request():
    """Pre-request processing for logging and security"""
    app.logger.info(f"{request.method} {request.path} from {request.remote_addr}")

@app.after_request
def after_request(response):
    """Post-request processing for logging"""
    app.logger.info(f"Response: {response.status_code} for {request.path}")
    return response

@app.route('/firmware/manifest', methods=['GET'])
@limiter.limit(Config.RATE_LIMIT_BURST)
def get_manifest():
    """
    Get firmware manifest with latest version information
    
    Returns:
        JSON response with firmware manifest or error
    """
    # Validate API key
    if not validate_api_key(request):
        app.logger.warning(f"Unauthorized manifest request from {request.remote_addr}")
        abort(401)
    
    try:
        # Get latest firmware info
        firmware_info = firmware_manager.get_latest_firmware()
        
        if not firmware_info:
            app.logger.error("No firmware available")
            return jsonify({"error": "No firmware available"}), 404
        
        # Log device request
        device_id = request.headers.get('X-Device-ID', 'unknown')
        app.logger.info(f"Manifest requested by device: {device_id}")
        
        # Return manifest
        response_data = {
            "version": firmware_info["version"],
            "download_url": firmware_info["download_url"],
            "sha256": firmware_info["sha256"],
            "hmac": firmware_info["hmac"],
            "size": firmware_info["size"],
            "force_update": firmware_info.get("force_update", False),
            "server_time": datetime.now().isoformat()
        }
        
        return jsonify(response_data)
        
    except Exception as e:
        app.logger.error(f"Error serving manifest: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/firmware/<version>.bin', methods=['GET'])
@limiter.limit(Config.RATE_LIMIT_BURST)
def download_firmware(version):
    """
    Download specific firmware version
    
    Args:
        version: Firmware version to download
        
    Returns:
        Binary firmware file or error response
    """
    # Validate API key
    if not validate_api_key(request):
        app.logger.warning(f"Unauthorized firmware download from {request.remote_addr}")
        abort(401)
    
    try:
        # Validate version format (security check)
        if not secure_filename_validation(version):
            app.logger.warning(f"Invalid version format: {version}")
            abort(400)
        
        # Get firmware path
        firmware_path = firmware_manager.get_firmware_path(version)
        
        if not firmware_path:
            app.logger.warning(f"Firmware version {version} not found")
            abort(404)
        
        # Log download
        device_id = request.headers.get('X-Device-ID', 'unknown')
        app.logger.info(f"Firmware {version} downloaded by device: {device_id}")
        
        # Send file
        return send_file(
            firmware_path,
            as_attachment=True,
            download_name=f"{version}.bin",
            mimetype='application/octet-stream'
        )
        
    except Exception as e:
        app.logger.error(f"Error serving firmware {version}: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/firmware/report', methods=['POST'])
@limiter.limit(Config.RATE_LIMIT_BURST)
def report_status():
    """
    Receive device update status reports
    
    Returns:
        JSON acknowledgment response
    """
    # Validate API key
    if not validate_api_key(request):
        app.logger.warning(f"Unauthorized status report from {request.remote_addr}")
        abort(401)
    
    try:
        # Parse request data
        if not request.is_json:
            return jsonify({"error": "Content-Type must be application/json"}), 400
        
        status_data = request.get_json()
        
        # Validate required fields
        required_fields = ['device_id', 'status']
        for field in required_fields:
            if field not in status_data:
                return jsonify({"error": f"Missing required field: {field}"}), 400
        
        # Extract device information
        device_id = status_data['device_id']
        status = status_data['status']
        
        # Update device status
        device_tracker.update_device_status(device_id, status_data)
        
        app.logger.info(f"Status report from {device_id}: {status}")
        
        # Return acknowledgment
        return jsonify({
            "status": "received",
            "timestamp": datetime.now().isoformat(),
            "device_id": device_id
        })
        
    except Exception as e:
        app.logger.error(f"Error processing status report: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/admin/devices', methods=['GET'])
@limiter.limit("10 per minute")
def admin_devices():
    """
    Administrative endpoint to view all device statuses
    
    Returns:
        JSON with all device status information
    """
    # Validate API key
    if not validate_api_key(request):
        abort(401)
    
    try:
        devices = device_tracker.get_all_devices()
        return jsonify({
            "devices": devices,
            "count": len(devices),
            "timestamp": datetime.now().isoformat()
        })
    except Exception as e:
        app.logger.error(f"Error retrieving device list: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/admin/history', methods=['GET'])
@limiter.limit("10 per minute")
def admin_history():
    """
    Administrative endpoint to view update history
    
    Returns:
        JSON with recent update history
    """
    # Validate API key
    if not validate_api_key(request):
        abort(401)
    
    try:
        limit = request.args.get('limit', 100, type=int)
        limit = min(max(limit, 1), 1000)  # Clamp between 1 and 1000
        
        history = device_tracker.get_update_history(limit)
        return jsonify({
            "history": history,
            "count": len(history),
            "timestamp": datetime.now().isoformat()
        })
    except Exception as e:
        app.logger.error(f"Error retrieving update history: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/admin/upload', methods=['POST'])
@limiter.limit("5 per minute")
def admin_upload_firmware():
    """
    Administrative endpoint to upload new firmware
    
    Returns:
        JSON with upload status and firmware information
    """
    # Validate API key
    if not validate_api_key(request):
        abort(401)
    
    try:
        # Check if file was uploaded
        if 'firmware' not in request.files:
            return jsonify({"error": "No firmware file provided"}), 400
        
        file = request.files['firmware']
        version = request.form.get('version')
        
        if not version:
            return jsonify({"error": "Version parameter required"}), 400
        
        if file.filename == '':
            return jsonify({"error": "No file selected"}), 400
        
        # Validate filename
        if not secure_filename_validation(file.filename):
            return jsonify({"error": "Invalid filename"}), 400
        
        # Check file size
        file.seek(0, os.SEEK_END)
        file_size = file.tell()
        file.seek(0)
        
        if file_size > Config.MAX_FIRMWARE_SIZE:
            return jsonify({"error": "Firmware file too large"}), 400
        
        # Save uploaded file
        upload_dir = Path(Config.UPLOAD_DIR)
        temp_filename = f"{version}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"
        temp_filepath = upload_dir / temp_filename
        
        file.save(str(temp_filepath))
        
        # Move to firmware directory with proper name
        firmware_filename = f"{version}.bin"
        firmware_filepath = Path(Config.FIRMWARE_DIR) / firmware_filename
        
        # Remove existing version if it exists
        if firmware_filepath.exists():
            firmware_filepath.unlink()
        
        temp_filepath.rename(firmware_filepath)
        
        # Add to firmware manager
        firmware_info = firmware_manager.add_firmware(version, str(firmware_filepath))
        
        app.logger.info(f"New firmware uploaded: {version}")
        
        return jsonify({
            "status": "uploaded",
            "firmware": firmware_info,
            "timestamp": datetime.now().isoformat()
        })
        
    except Exception as e:
        app.logger.error(f"Error uploading firmware: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/health', methods=['GET'])
def health_check():
    """
    Health check endpoint
    
    Returns:
        JSON with server health status
    """
    return jsonify({
        "status": "healthy",
        "timestamp": datetime.now().isoformat(),
        "version": "1.0.0"
    })

@app.errorhandler(401)
def unauthorized(error):
    """Handle unauthorized access"""
    return jsonify({"error": "Unauthorized access"}), 401

@app.errorhandler(404)
def not_found(error):
    """Handle not found errors"""
    return jsonify({"error": "Resource not found"}), 404

@app.errorhandler(429)
def rate_limit_exceeded(error):
    """Handle rate limit exceeded errors"""
    return jsonify({"error": "Rate limit exceeded"}), 429

@app.errorhandler(500)
def internal_error(error):
    """Handle internal server errors"""
    app.logger.error(f"Internal server error: {error}")
    return jsonify({"error": "Internal server error"}), 500

# ========== SSL Certificate Generation ==========
def generate_self_signed_cert():
    """
    Generate self-signed SSL certificate for development
    
    Note: In production, use proper CA-signed certificates
    """
    try:
        from cryptography import x509
        from cryptography.x509.oid import NameOID
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.backends import default_backend
        from cryptography.hazmat.primitives.asymmetric import rsa
        from cryptography.hazmat.primitives import serialization
        import ipaddress
        
        # Generate private key
        private_key = rsa.generate_private_key(
            public_exponent=65537,
            key_size=2048,
            backend=default_backend()
        )
        
        # Create certificate
        subject = issuer = x509.Name([
            x509.NameAttribute(NameOID.COUNTRY_NAME, "US"),
            x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, "CA"),
            x509.NameAttribute(NameOID.LOCALITY_NAME, "San Francisco"),
            x509.NameAttribute(NameOID.ORGANIZATION_NAME, "ESP32 OTA Server"),
            x509.NameAttribute(NameOID.COMMON_NAME, "localhost"),
        ])
        
        cert = x509.CertificateBuilder().subject_name(
            subject
        ).issuer_name(
            issuer
        ).public_key(
            private_key.public_key()
        ).serial_number(
            x509.random_serial_number()
        ).not_valid_before(
            datetime.utcnow()
        ).not_valid_after(
            datetime.utcnow() + timedelta(days=365)
        ).add_extension(
            x509.SubjectAlternativeName([
                x509.DNSName("localhost"),
                x509.IPAddress(ipaddress.ip_address("127.0.0.1")),
                x509.IPAddress(ipaddress.ip_address("192.168.1.100")),
            ]),
            critical=False,
        ).sign(private_key, hashes.SHA256(), default_backend())
        
        # Write certificate and key to files
        with open(Config.SSL_CERT_FILE, "wb") as f:
            f.write(cert.public_bytes(serialization.Encoding.PEM))
        
        with open(Config.SSL_KEY_FILE, "wb") as f:
            f.write(private_key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption()
            ))
        
        print(f"Generated self-signed certificate: {Config.SSL_CERT_FILE}")
        print(f"Generated private key: {Config.SSL_KEY_FILE}")
        
    except ImportError:
        print("cryptography library not installed. Install with: pip install cryptography")
        print("Using HTTP mode instead of HTTPS")
        Config.USE_SSL = False
    except Exception as e:
        print(f"Error generating certificate: {e}")
        Config.USE_SSL = False

# ========== Main Application Entry Point ==========
def main():
    """Main application entry point"""
    
    print("=" * 60)
    print("ESP32 Secure OTA Server")
    print("Version: 1.0.0")
    print("=" * 60)
    
    # Check for SSL certificates
    if Config.USE_SSL:
        if not os.path.exists(Config.SSL_CERT_FILE) or not os.path.exists(Config.SSL_KEY_FILE):
            print("SSL certificates not found. Generating self-signed certificates...")
            generate_self_signed_cert()
    
    # Print configuration
    protocol = "HTTPS" if Config.USE_SSL else "HTTP"
    print(f"Server will start on {protocol}://{Config.HOST}:{Config.PORT}")
    print(f"Firmware directory: {os.path.abspath(Config.FIRMWARE_DIR)}")
    print(f"Upload directory: {os.path.abspath(Config.UPLOAD_DIR)}")
    print(f"Log file: {os.path.abspath(Config.LOG_FILE)}")
    
    if Config.USE_SSL:
        print(f"SSL Certificate: {os.path.abspath(Config.SSL_CERT_FILE)}")
        print(f"SSL Private Key: {os.path.abspath(Config.SSL_KEY_FILE)}")
    
    print("\nAPI Endpoints:")
    print("  GET  /firmware/manifest     - Get firmware manifest")
    print("  GET  /firmware/<version>.bin - Download firmware")
    print("  POST /firmware/report       - Device status report")
    print("  GET  /admin/devices         - List all devices")
    print("  GET  /admin/history         - Update history")
    print("  POST /admin/upload          - Upload firmware")
    print("  GET  /health                - Health check")
    
    print(f"\nAPI Key: {Config.API_KEY}")
    print("WARNING: Change the API key in production!")
    
    print("\n" + "=" * 60)
    
    # Start server
    try:
        if Config.USE_SSL and os.path.exists(Config.SSL_CERT_FILE):
            context = ssl.SSLContext(ssl.PROTOCOL_TLSv1_2)
            context.load_cert_chain(Config.SSL_CERT_FILE, Config.SSL_KEY_FILE)
            app.run(
                host=Config.HOST,
                port=Config.PORT,
                debug=Config.DEBUG,
                ssl_context=context
            )
        else:
            print("Running in HTTP mode (SSL disabled)")
            app.run(
                host=Config.HOST,
                port=Config.PORT,
                debug=Config.DEBUG
            )
    except KeyboardInterrupt:
        print("\nShutting down server...")
    except Exception as e:
        print(f"Server error: {e}")

if __name__ == '__main__':
    main()