"""
M4 - Remote Configuration Tests for Flask Server

Tests dynamic runtime configuration updates for ESP32 devices:
- Configuration update endpoint
- Parameter validation (frequencies, register selection)
- Success/failure reporting
- Configuration history logging
- Idempotent updates
- Concurrent config updates
- Configuration retrieval
- Error handling and validation

Author: Team PowerPort
Date: 2025-01-22
"""

import pytest
import json
import time
from flask import Flask
from flask.testing import FlaskClient
import os
import sys

# Add parent directory to path for imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))


# Mock config storage (in real implementation, would be database/redis)
config_store = {}
config_history = []


def create_test_app():
    """Create Flask app with config routes for testing"""
    app = Flask(__name__)
    app.config['TESTING'] = True
    
    @app.route('/config/update', methods=['POST'])
    def update_config():
        """Update device configuration"""
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        device_id = data.get('device_id')
        if not device_id:
            return jsonify({'error': 'device_id is required'}), 400
        
        # Validate configuration parameters
        poll_freq = data.get('poll_frequency')
        upload_freq = data.get('upload_frequency')
        registers = data.get('registers')
        
        errors = []
        
        # Validate poll frequency (1 second to 1 hour in microseconds)
        if poll_freq is not None:
            if not isinstance(poll_freq, int) or poll_freq < 1000000 or poll_freq > 3600000000:
                errors.append('poll_frequency must be between 1000000 and 3600000000 microseconds')
        
        # Validate upload frequency
        if upload_freq is not None:
            if not isinstance(upload_freq, int) or upload_freq < 1000000 or upload_freq > 3600000000:
                errors.append('upload_frequency must be between 1000000 and 3600000000 microseconds')
        
        # Validate registers (list of register IDs)
        if registers is not None:
            if not isinstance(registers, list):
                errors.append('registers must be a list')
            elif len(registers) > 16:
                errors.append('registers list cannot exceed 16 entries')
        
        if errors:
            return jsonify({'error': 'Validation failed', 'details': errors}), 400
        
        # Store configuration
        if device_id not in config_store:
            config_store[device_id] = {}
        
        updated_fields = []
        if poll_freq is not None:
            config_store[device_id]['poll_frequency'] = poll_freq
            updated_fields.append('poll_frequency')
        if upload_freq is not None:
            config_store[device_id]['upload_frequency'] = upload_freq
            updated_fields.append('upload_frequency')
        if registers is not None:
            config_store[device_id]['registers'] = registers
            updated_fields.append('registers')
        
        # Log to history
        history_entry = {
            'device_id': device_id,
            'timestamp': time.time(),
            'updated_fields': updated_fields,
            'config': config_store[device_id].copy()
        }
        config_history.append(history_entry)
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'updated_fields': updated_fields,
            'config': config_store[device_id]
        }), 200
    
    @app.route('/config/current/<device_id>', methods=['GET'])
    def get_current_config(device_id):
        """Get current configuration for a device"""
        if device_id not in config_store:
            return jsonify({'error': 'Device not found'}), 404
        
        return jsonify({
            'device_id': device_id,
            'config': config_store[device_id]
        }), 200
    
    @app.route('/config/history/<device_id>', methods=['GET'])
    def get_config_history(device_id):
        """Get configuration update history for a device"""
        device_history = [h for h in config_history if h['device_id'] == device_id]
        return jsonify({
            'device_id': device_id,
            'history': device_history
        }), 200
    
    return app


@pytest.fixture
def app():
    """Create and configure test app"""
    return create_test_app()


@pytest.fixture
def client(app):
    """Flask test client"""
    # Import here to avoid circular imports
    global request, jsonify
    from flask import request, jsonify
    
    # Clear stores before each test
    config_store.clear()
    config_history.clear()
    
    return app.test_client()


class TestRemoteConfiguration:
    """Test suite for remote configuration management"""
    
    def test_update_poll_frequency(self, client: FlaskClient):
        """Test 1: Update device polling frequency"""
        response = client.post('/config/update', 
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 5000000  # 5 seconds
            })
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert data['device_id'] == 'ESP32_CONFIG_TEST'
        assert 'poll_frequency' in data['updated_fields']
        assert data['config']['poll_frequency'] == 5000000
    
    def test_update_upload_frequency(self, client: FlaskClient):
        """Test 2: Update device upload frequency"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'upload_frequency': 10000000  # 10 seconds
            })
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert 'upload_frequency' in data['updated_fields']
        assert data['config']['upload_frequency'] == 10000000
    
    def test_update_register_list(self, client: FlaskClient):
        """Test 3: Update device register monitoring list"""
        registers = [0, 1, 2, 3, 4, 5]  # First 6 registers
        
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'registers': registers
            })
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert 'registers' in data['updated_fields']
        assert data['config']['registers'] == registers
    
    def test_multiple_parameter_update(self, client: FlaskClient):
        """Test 4: Update multiple parameters simultaneously"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 6000000,  # 6 seconds
                'upload_frequency': 12000000,  # 12 seconds
                'registers': [0, 1, 2, 3, 4, 5]
            })
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert len(data['updated_fields']) == 3
        assert 'poll_frequency' in data['updated_fields']
        assert 'upload_frequency' in data['updated_fields']
        assert 'registers' in data['updated_fields']
    
    def test_validation_invalid_poll_frequency_too_small(self, client: FlaskClient):
        """Test 5: Reject poll frequency below minimum"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 500000  # 0.5 seconds (too small)
            })
        
        assert response.status_code == 400
        data = response.get_json()
        assert 'error' in data
        assert 'Validation failed' in data['error']
    
    def test_validation_invalid_poll_frequency_too_large(self, client: FlaskClient):
        """Test 6: Reject poll frequency above maximum"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 4000000000  # Over 1 hour (too large)
            })
        
        assert response.status_code == 400
        data = response.get_json()
        assert 'error' in data
    
    def test_validation_invalid_registers_not_list(self, client: FlaskClient):
        """Test 7: Reject registers parameter if not a list"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'registers': "0,1,2,3"  # String instead of list
            })
        
        assert response.status_code == 400
        data = response.get_json()
        assert 'error' in data
        assert any('list' in detail for detail in data['details'])
    
    def test_validation_registers_too_many(self, client: FlaskClient):
        """Test 8: Reject register list exceeding maximum"""
        registers = list(range(20))  # 20 registers (too many)
        
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'registers': registers
            })
        
        assert response.status_code == 400
        data = response.get_json()
        assert 'error' in data
        assert any('cannot exceed 16' in detail for detail in data['details'])
    
    def test_idempotent_updates(self, client: FlaskClient):
        """Test 9: Same configuration update twice should succeed"""
        config = {
            'device_id': 'ESP32_CONFIG_TEST',
            'poll_frequency': 7000000
        }
        
        # First update
        response1 = client.post('/config/update', json=config)
        assert response1.status_code == 200
        data1 = response1.get_json()
        
        # Second update with same values
        response2 = client.post('/config/update', json=config)
        assert response2.status_code == 200
        data2 = response2.get_json()
        
        assert data1['config']['poll_frequency'] == data2['config']['poll_frequency']
    
    def test_get_current_config(self, client: FlaskClient):
        """Test 10: Retrieve current device configuration"""
        # Set configuration
        client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 8000000,
                'upload_frequency': 16000000
            })
        
        # Retrieve configuration
        response = client.get('/config/current/ESP32_CONFIG_TEST')
        assert response.status_code == 200
        data = response.get_json()
        assert data['device_id'] == 'ESP32_CONFIG_TEST'
        assert data['config']['poll_frequency'] == 8000000
        assert data['config']['upload_frequency'] == 16000000
    
    def test_get_config_not_found(self, client: FlaskClient):
        """Test 11: Return 404 for non-existent device"""
        response = client.get('/config/current/NONEXISTENT_DEVICE')
        assert response.status_code == 404
        data = response.get_json()
        assert 'error' in data
    
    def test_config_history_logging(self, client: FlaskClient):
        """Test 12: Configuration changes are logged to history"""
        device_id = 'ESP32_CONFIG_TEST'
        
        # First update
        client.post('/config/update',
            json={
                'device_id': device_id,
                'poll_frequency': 5000000
            })
        
        # Second update
        client.post('/config/update',
            json={
                'device_id': device_id,
                'upload_frequency': 10000000
            })
        
        # Get history
        response = client.get(f'/config/history/{device_id}')
        assert response.status_code == 200
        data = response.get_json()
        assert len(data['history']) == 2
        assert data['history'][0]['updated_fields'] == ['poll_frequency']
        assert data['history'][1]['updated_fields'] == ['upload_frequency']
    
    def test_concurrent_device_configs(self, client: FlaskClient):
        """Test 13: Multiple devices can have independent configurations"""
        # Configure device 1
        client.post('/config/update',
            json={
                'device_id': 'ESP32_DEVICE_1',
                'poll_frequency': 4000000
            })
        
        # Configure device 2
        client.post('/config/update',
            json={
                'device_id': 'ESP32_DEVICE_2',
                'poll_frequency': 6000000
            })
        
        # Verify device 1
        response1 = client.get('/config/current/ESP32_DEVICE_1')
        assert response1.get_json()['config']['poll_frequency'] == 4000000
        
        # Verify device 2
        response2 = client.get('/config/current/ESP32_DEVICE_2')
        assert response2.get_json()['config']['poll_frequency'] == 6000000
    
    def test_missing_device_id(self, client: FlaskClient):
        """Test 14: Reject request without device_id"""
        response = client.post('/config/update',
            json={
                'poll_frequency': 5000000
            })
        
        assert response.status_code == 400
        data = response.get_json()
        assert 'device_id is required' in data['error']
    
    def test_edge_values_minimum_frequency(self, client: FlaskClient):
        """Test 15: Accept minimum valid frequency (1 second)"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 1000000  # Exactly 1 second
            })
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['config']['poll_frequency'] == 1000000
    
    def test_edge_values_maximum_frequency(self, client: FlaskClient):
        """Test 16: Accept maximum valid frequency (1 hour)"""
        response = client.post('/config/update',
            json={
                'device_id': 'ESP32_CONFIG_TEST',
                'poll_frequency': 3600000000  # Exactly 1 hour
            })
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['config']['poll_frequency'] == 3600000000


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
