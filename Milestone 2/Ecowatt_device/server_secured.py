#!/usr/bin/env python3
"""
EcoWatt Security Layer - Server-side Verification Example
This server can verify HMAC-SHA256 secured messages from the ESP32
"""

import json
import hmac
import hashlib
import base64
from flask import Flask, request, jsonify

app = Flask(__name__)

# Pre-shared key (must match ESP32) - 64 hex characters (32 bytes)
PSK_HEX = "4A6F486E20446F652041455336342D536563726574204B65792D323536626974"
PSK_BYTES = bytes.fromhex(PSK_HEX)

# Anti-replay protection
last_valid_nonce = 0

def verify_secured_message(secured_message_str):
    """
    Verify a secured message from ESP32
    Returns (success, payload) where success is bool and payload is the original JSON
    """
    global last_valid_nonce
    
    try:
        # Parse the secured message
        secured_msg = json.loads(secured_message_str)
        
        # Validate structure
        required_fields = ['nonce', 'payload', 'mac', 'encrypted']
        for field in required_fields:
            if field not in secured_msg:
                return False, f"Missing field: {field}"
        
        nonce = secured_msg['nonce']
        payload = secured_msg['payload']
        received_mac = secured_msg['mac']
        is_encrypted = secured_msg['encrypted']
        
        # Validate nonce (anti-replay protection)
        if nonce <= last_valid_nonce:
            return False, f"Invalid nonce {nonce} (replay attack? last valid: {last_valid_nonce})"
        
        # Calculate HMAC for verification
        data_for_hmac = f"{nonce}:{payload}"
        calculated_hmac = hmac.new(
            PSK_BYTES, 
            data_for_hmac.encode('utf-8'), 
            hashlib.sha256
        ).hexdigest()
        
        # Verify HMAC
        if received_mac != calculated_hmac:
            return False, "HMAC verification failed - message tampered!"
        
        # Update last valid nonce
        last_valid_nonce = nonce
        
        # Decrypt payload if encrypted (mock decryption)
        if is_encrypted:
            payload = mock_decrypt(payload)
        
        return True, payload
        
    except json.JSONDecodeError:
        return False, "Invalid JSON format"
    except Exception as e:
        return False, f"Verification error: {str(e)}"

def mock_decrypt(encrypted_data):
    """
    Mock decryption function that reverses the ESP32's mock encryption
    This is a simple demonstration - use proper encryption in production
    """
    try:
        # Reverse character shifting
        shifted = ""
        for c in encrypted_data:
            if 'A' <= c <= 'Z':
                c = chr(((ord(c) - ord('A') - 3 + 26) % 26) + ord('A'))
            elif 'a' <= c <= 'z':
                c = chr(((ord(c) - ord('a') - 3 + 26) % 26) + ord('a'))
            elif '0' <= c <= '9':
                c = chr(((ord(c) - ord('0') - 3 + 10) % 10) + ord('0'))
            shifted += c
        
        # Base64 decode
        decoded = base64.b64decode(shifted).decode('utf-8')
        return decoded
        
    except Exception as e:
        print(f"Mock decryption failed: {e}")
        return encrypted_data  # Return as-is if decryption fails

@app.route('/process', methods=['POST'])
def process_data():
    """
    Main endpoint to receive and process secured data from ESP32
    """
    try:
        # Get the secured payload
        secured_payload = request.get_data(as_text=True)
        
        print(f"\n=== Received secured payload ===")
        print(f"Size: {len(secured_payload)} bytes")
        
        # Verify the secured message
        success, result = verify_secured_message(secured_payload)
        
        if not success:
            print(f"❌ Security verification failed: {result}")
            return jsonify({
                "status": "error",
                "message": f"Security verification failed: {result}"
            }), 400
        
        print("✅ Security verification passed")
        print(f"Nonce: {last_valid_nonce}")
        
        # Parse the original payload
        original_data = json.loads(result)
        
        # Process the data
        device_id = original_data.get('device_id', 'unknown')
        data_type = original_data.get('data_type', 'unknown')
        total_samples = original_data.get('total_samples', 0)
        
        print(f"📊 Processing data from {device_id}")
        print(f"   Type: {data_type}")
        print(f"   Samples: {total_samples}")
        
        # Extract compression summary if available
        if 'session_summary' in original_data:
            summary = original_data['session_summary']
            print(f"   Compression: {summary.get('overall_savings_percent', 0):.1f}% savings")
            print(f"   Optimal method: {summary.get('optimal_method', 'unknown')}")
        
        # Here you would process the decompressed sensor data
        # For demonstration, we'll just acknowledge receipt
        
        return jsonify({
            "status": "success",
            "message": "Secured data received and verified",
            "device_id": device_id,
            "nonce": last_valid_nonce,
            "samples_received": total_samples
        }), 200
        
    except Exception as e:
        print(f"❌ Server error: {str(e)}")
        return jsonify({
            "status": "error",
            "message": f"Server error: {str(e)}"
        }), 500

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        "status": "healthy",
        "security_enabled": True,
        "last_nonce": last_valid_nonce
    })

if __name__ == '__main__':
    print("🔒 EcoWatt Secured Server Starting...")
    print(f"🔑 PSK: {PSK_HEX[:16]}...{PSK_HEX[-16:]} (truncated)")
    print("🛡️  HMAC-SHA256 verification enabled")
    print("🚫 Anti-replay protection active")
    print("=" * 50)
    
    # Run Flask server
    app.run(
        host='0.0.0.0',  # Accept connections from any IP
        port=5001,       # Match the ESP32 serverURL port
        debug=True
    )