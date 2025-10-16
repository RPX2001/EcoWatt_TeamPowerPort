"""
Security Manager for Flask Server
Implements HMAC-SHA256 authentication and anti-replay protection
"""

import hmac
import hashlib
import json
from datetime import datetime
import threading

class SecurityManager:
    def __init__(self, psk_hex):
        """
        Initialize security manager with pre-shared key.
        
        Args:
            psk_hex: Hex string of PSK (must match ESP32)
        """
        self.psk = bytes.fromhex(psk_hex)
        self.device_sequences = {}  # Track sequence numbers per device
        self.lock = threading.Lock()
        self.validation_log = []  # Log of all validations
        
    def compute_hmac(self, data, sequence_number):
        """
        Compute HMAC-SHA256 for data + sequence number.
        
        Args:
            data: Payload data (bytes or string)
            sequence_number: Sequence number (int)
            
        Returns:
            HMAC as hex string
        """
        if isinstance(data, str):
            data = data.encode('utf-8')
        
        # Append sequence number (big-endian, 4 bytes)
        sequence_bytes = sequence_number.to_bytes(4, byteorder='big')
        message = data + sequence_bytes
        
        # Compute HMAC
        h = hmac.new(self.psk, message, hashlib.sha256)
        return h.hexdigest()
    
    def validate_request(self, device_id, payload, sequence_number, received_hmac):
        """
        Validate incoming request with HMAC and anti-replay check.
        
        Args:
            device_id: Device identifier
            payload: Request payload (dict or string)
            sequence_number: Sequence number from request
            received_hmac: HMAC from request header
            
        Returns:
            (is_valid, error_message)
        """
        with self.lock:
            # Convert payload to bytes if needed
            if isinstance(payload, dict):
                payload_str = json.dumps(payload, separators=(',', ':'))
                payload_bytes = payload_str.encode('utf-8')
            elif isinstance(payload, str):
                payload_bytes = payload.encode('utf-8')
            else:
                payload_bytes = payload
            
            # Compute expected HMAC
            expected_hmac = self.compute_hmac(payload_bytes, sequence_number)
            
            # Check HMAC
            if not hmac.compare_digest(expected_hmac, received_hmac):
                error = f"HMAC validation failed for device {device_id}"
                self._log_validation(device_id, sequence_number, False, error)
                return False, error
            
            # Check sequence number (anti-replay)
            last_sequence = self.device_sequences.get(device_id, -1)
            
            if sequence_number <= last_sequence:
                error = f"Replay attack detected! Sequence {sequence_number} <= last {last_sequence}"
                self._log_validation(device_id, sequence_number, False, error)
                return False, error
            
            # Update sequence number
            self.device_sequences[device_id] = sequence_number
            
            # Log success
            self._log_validation(device_id, sequence_number, True, "OK")
            
            return True, "Valid"
    
    def _log_validation(self, device_id, sequence, valid, message):
        """Log validation attempt."""
        entry = {
            'timestamp': datetime.now().isoformat(),
            'device_id': device_id,
            'sequence': sequence,
            'valid': valid,
            'message': message
        }
        self.validation_log.append(entry)
        
        # Keep only last 1000 entries
        if len(self.validation_log) > 1000:
            self.validation_log = self.validation_log[-1000:]
    
    def get_device_sequence(self, device_id):
        """Get last valid sequence number for device."""
        with self.lock:
            return self.device_sequences.get(device_id, 0)
    
    def reset_device_sequence(self, device_id):
        """Reset sequence number for device (testing only)."""
        with self.lock:
            self.device_sequences[device_id] = 0
            self._log_validation(device_id, 0, True, "Sequence reset")
    
    def get_validation_log(self, device_id=None, limit=50):
        """Get validation log entries."""
        with self.lock:
            if device_id:
                filtered = [entry for entry in self.validation_log if entry['device_id'] == device_id]
                return filtered[-limit:]
            else:
                return self.validation_log[-limit:]
    
    def get_stats(self):
        """Get security statistics."""
        with self.lock:
            total = len(self.validation_log)
            valid = sum(1 for entry in self.validation_log if entry['valid'])
            invalid = total - valid
            
            return {
                'total_validations': total,
                'valid_requests': valid,
                'invalid_requests': invalid,
                'devices_tracked': len(self.device_sequences),
                'device_sequences': dict(self.device_sequences)
            }


# PSK (must match ESP32 credentials.h)
PSK_HEX = "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe"

# Global security manager instance
security_manager = SecurityManager(PSK_HEX)
