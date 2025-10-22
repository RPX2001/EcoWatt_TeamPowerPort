"""
M3 Flask Integration Tests - Simplified Working Version
Tests the Flask server with correct API format (aggregated_data)
"""

import pytest
import json
import time

try:
    from flask_server_modular import app
except ImportError:
    from sys import path
    path.insert(0, '../..')
    from flask_server_modular import app

from utils.logger_utils import get_logger

logger = get_logger(__name__)

# Test configuration
TEST_DEVICE_ID = "TEST_ESP32_M3"


@pytest.fixture
def client():
    """Flask test client fixture"""
    app.config['TESTING'] = True
    with app.app_context():
        with app.test_client() as client:
            yield client


@pytest.fixture
def sample_aggregated_data():
    """Generate sample aggregated data (JSON list format)"""
    return [
        {
            'voltage': 5000 + i, 
            'current': 500 + i//10, 
            'power': 2000 + i*2, 
            'timestamp': int(time.time() * 1000) + i
        }
        for i in range(60)
    ]


class TestM3FlaskSimple:
    """M3 Flask Integration Test Suite - Simplified"""
    
    def test_flask_health(self, client):
        """Test 1: Flask server health"""
        logger.info("=== Test: Server Health ===")
        
        response = client.get('/health')
        assert response.status_code == 200
        
        data = json.loads(response.data)
        assert data['status'] == 'healthy'
        
        logger.info("[PASS] Server healthy")
    
    def test_receive_aggregated_data(self, client, sample_aggregated_data):
        """Test 2: Receive aggregated data"""
        logger.info("=== Test: Receive Data ===")
        
        payload = {'aggregated_data': sample_aggregated_data[:10]}
        
        response = client.post(f'/aggregated/{TEST_DEVICE_ID}', json=payload)
        assert response.status_code in [200, 201]
        
        data = json.loads(response.data)
        assert data['success'] is True
        assert data['samples_count'] == 10
        
        logger.info(f"[PASS] Received {data['samples_count']} samples")
    
    def test_multiple_devices(self, client, sample_aggregated_data):
        """Test 3: Multiple devices"""
        logger.info("=== Test: Multiple Devices ===")
        
        for device_id in ["ESP32_001", "ESP32_002", "ESP32_003"]:
            payload = {'aggregated_data': sample_aggregated_data[:5]}
            response = client.post(f'/aggregated/{device_id}', json=payload)
            assert response.status_code in [200, 201]
        
        logger.info("[PASS] Multiple devices supported")
    
    def test_large_batch(self, client):
        """Test 4: Large batch (900 samples)"""
        logger.info("=== Test: Large Batch ===")
        
        large_data = [
            {'voltage': 5000+i, 'current': 500, 'power': 2000+i, 'timestamp': int(time.time()*1000)+i}
            for i in range(900)
        ]
        
        payload = {'aggregated_data': large_data}
        response = client.post(f'/aggregated/{TEST_DEVICE_ID}', json=payload)
        
        assert response.status_code in [200, 201]
        data = json.loads(response.data)
        assert data['samples_count'] == 900
        
        logger.info("[PASS] Large batch uploaded")
    
    def test_invalid_data(self, client):
        """Test 5: Invalid data rejection"""
        logger.info("=== Test: Invalid Data ===")
        
        # Missing aggregated_data
        response1 = client.post(f'/aggregated/{TEST_DEVICE_ID}', json={'timestamp': 123})
        assert response1.status_code in [400, 422]
        
        # Non-list aggregated_data
        response2 = client.post(f'/aggregated/{TEST_DEVICE_ID}', json={'aggregated_data': "invalid"})
        assert response2.status_code in [400, 422]
        
        logger.info("[PASS] Invalid data rejected")
    
    def test_compression_stats(self, client):
        """Test 6: Compression statistics"""
        logger.info("=== Test: Compression Stats ===")
        
        response = client.get('/compression/stats')
        assert response.status_code == 200
        
        stats = json.loads(response.data)
        assert isinstance(stats, dict)
        
        logger.info("[PASS] Stats endpoint working")


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
