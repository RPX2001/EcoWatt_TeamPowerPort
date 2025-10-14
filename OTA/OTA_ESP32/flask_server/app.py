from flask import Flask, send_file, jsonify, request
import os
import json
import hashlib
from pathlib import Path
from datetime import datetime

app = Flask(__name__)

# Directory to store firmware files
FIRMWARE_DIR = Path(__file__).parent / "firmware"
FIRMWARE_DIR.mkdir(exist_ok=True)

# Manifest file to track firmware metadata
MANIFEST_FILE = FIRMWARE_DIR / "manifest.json"

def calculate_sha256(file_path):
    """Calculate SHA256 hash of a file"""
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            sha256_hash.update(chunk)
    return sha256_hash.hexdigest()

def get_or_create_manifest():
    """Get existing manifest or create a default one"""
    if MANIFEST_FILE.exists():
        try:
            with open(MANIFEST_FILE, 'r') as f:
                return json.load(f)
        except (json.JSONDecodeError, FileNotFoundError):
            pass
    
    # Default manifest
    firmware_path = FIRMWARE_DIR / "latest.bin"
    if firmware_path.exists():
        manifest = {
            "version": "1.0.0",
            "sha256": calculate_sha256(firmware_path),
            "url": "/firmware/latest.bin",
            "size": firmware_path.stat().st_size,
            "updated_at": datetime.now().isoformat()
        }
        save_manifest(manifest)
        return manifest
    else:
        return {
            "version": "0.0.0",
            "sha256": "",
            "url": "/firmware/latest.bin",
            "size": 0,
            "updated_at": datetime.now().isoformat()
        }

def save_manifest(manifest):
    """Save manifest to file"""
    with open(MANIFEST_FILE, 'w') as f:
        json.dump(manifest, f, indent=2)

@app.route('/')
def index():
    """Basic info about the OTA server"""
    return jsonify({
        "message": "ESP32 OTA Update Server",
        "version": "1.0.0",
        "endpoints": {
            "manifest": "/manifest",
            "firmware": "/firmware/latest.bin",
            "info": "/info"
        }
    })

@app.route('/manifest')
def get_manifest():
    """Get firmware manifest with version, hash, and URL"""
    firmware_path = FIRMWARE_DIR / "latest.bin"
    
    if not firmware_path.exists():
        return jsonify({
            "error": "No firmware available",
            "version": "0.0.0",
            "sha256": "",
            "url": "/firmware/latest.bin",
            "size": 0
        }), 404
    
    manifest = get_or_create_manifest()
    
    # Verify the hash matches current file
    current_hash = calculate_sha256(firmware_path)
    if manifest.get("sha256") != current_hash:
        # Update manifest if hash doesn't match (file was updated externally)
        manifest["sha256"] = current_hash
        manifest["size"] = firmware_path.stat().st_size
        manifest["updated_at"] = datetime.now().isoformat()
        save_manifest(manifest)
    
    return jsonify(manifest)

@app.route('/firmware/latest.bin')
def get_latest_firmware():
    """Serve the latest firmware binary file"""
    firmware_path = FIRMWARE_DIR / "latest.bin"
    
    if not firmware_path.exists():
        return jsonify({"error": "Firmware file not found"}), 404
    
    return send_file(
        firmware_path,
        as_attachment=True,
        download_name="firmware.bin",
        mimetype="application/octet-stream"
    )

@app.route('/info')
def get_info():
    """Get information about available firmware"""
    firmware_path = FIRMWARE_DIR / "latest.bin"
    
    if firmware_path.exists():
        file_size = firmware_path.stat().st_size
        return jsonify({
            "firmware_available": True,
            "file_size": file_size,
            "file_path": str(firmware_path)
        })
    else:
        return jsonify({
            "firmware_available": False,
            "message": "No firmware file found"
        })

@app.route('/update-manifest', methods=['POST'])
def update_manifest():
    """Update manifest with new version info (called by deployment script)"""
    
    data = request.get_json()
    if not data or 'version' not in data:
        return jsonify({"error": "Version required"}), 400
    
    firmware_path = FIRMWARE_DIR / "latest.bin"
    if not firmware_path.exists():
        return jsonify({"error": "No firmware file found"}), 404
    
    manifest = {
        "version": data['version'],
        "sha256": calculate_sha256(firmware_path),
        "url": "/firmware/latest.bin",
        "size": firmware_path.stat().st_size,
        "updated_at": datetime.now().isoformat()
    }
    
    save_manifest(manifest)
    return jsonify({"message": "Manifest updated", "manifest": manifest})

@app.route('/upload', methods=['POST'])
def upload_firmware():
    """Simple endpoint to upload firmware (for future use)"""
    return jsonify({"message": "Upload endpoint not implemented yet"}), 501

if __name__ == '__main__':
    print("Starting ESP32 OTA Server...")
    print(f"Firmware directory: {FIRMWARE_DIR}")
    print("Server will be available at: http://localhost:5001")
    print("Firmware endpoint: http://localhost:5001/firmware/latest.bin")

    app.run(host='0.0.0.0', port=5001, debug=True)