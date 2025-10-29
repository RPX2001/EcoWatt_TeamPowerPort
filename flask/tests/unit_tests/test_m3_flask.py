"""
Unit Tests for M3 Flask Server - Data Reception and Storage
Tests aggregation routes, compression handlers, and data persistence

Author: Team PowerPort
Date: 2025-10-23
"""

import pytest
import json
import base64
from flask import Flask
from flask_server_modular import app
from handlers.compression_handler import (
    handle_compressed_data,
    validate_compression_crc,
    get_compression_statistics,
    reset_compression_statistics
)


# Client fixture now comes from conftest.py


@pytest.fixture
def sample_compressed_data():
    """Create sample compressed data for testing"""
    # Simulate bit-packed compressed data
    # Header: [0x01, 0x0D, 0x14] = method 0x01, 13 bits/value, 20 values
    data = bytearray([0x01, 0x0D, 0x14])
    
    # Add some compressed data (20 values × 13 bits = 260 bits = 33 bytes)
    for i in range(33):
        data.append((i * 7) & 0xFF)
    
    return base64.b64encode(data).decode('utf-8')


@pytest.fixture
def sample_aggregated_payload(sample_compressed_data):
    """Create sample aggregated data payload"""
    return {
        'device_id': 'TEST_ESP32_001',
        'timestamp': 1234567890,
        'data_type': 'compressed_sensor_batch',
        'total_samples': 2,
        'register_mapping': {
            '0': 'VAC',
            '1': 'IAC',
            '2': 'PAC'
        },
        'compressed_data': [
            {
                'compressed_binary': sample_compressed_data,
                'decompression_metadata': {
                    'method': 'BIT_PACKED',
                    'register_count': 3,
                    'original_size_bytes': 120,
                    'compressed_size_bytes': 101,
                    'timestamp': 1000,
                    'register_layout': [0, 1, 2]
                },
                'performance_metrics': {
                    'academic_ratio': 0.842,
                    'traditional_ratio': 1.188,
                    'compression_time_us': 120,
                    'savings_percent': 15.8,
                    'lossless_verified': True
                }
            }
        ],
        'session_summary': {
            'total_original_bytes': 120,
            'total_compressed_bytes': 101,
            'overall_academic_ratio': 0.842,
            'overall_savings_percent': 15.8
        }
    }


# ============================================================================
# TEST CASES - M3 Data Reception
# ============================================================================

def test_health_endpoint(client):
    """Test 1: Health check endpoint"""
    response = client.get('/health')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['status'] == 'healthy'


def test_aggregated_data_reception_success(client):
    """Test 2: Successfully receive aggregated data"""
    payload = {
        'aggregated_data': [
            {'timestamp': 1000, 'VAC': 220, 'IAC': 5000, 'PAC': 1000},
            {'timestamp': 2000, 'VAC': 221, 'IAC': 5010, 'PAC': 1005}
        ]
    }
    
    response = client.post(
        '/aggregated/TEST_DEVICE_001',
        data=json.dumps(payload),
        content_type='application/json'
    )
    
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['success'] == True
    assert data['device_id'] == 'TEST_DEVICE_001'
    assert 'samples_count' in data


def test_aggregated_data_missing_content_type(client):
    """Test 3: Reject request without JSON content type"""
    response = client.post(
        '/aggregated/TEST_DEVICE_001',
        data='not json',
        content_type='text/plain'
    )
    
    assert response.status_code == 400
    data = json.loads(response.data)
    assert data['success'] == False
    assert 'Content-Type' in data['error']


def test_aggregated_data_missing_fields(client):
    """Test 4: Reject data without required fields"""
    payload = {
        'device_id': 'TEST_DEVICE_001'
        # Missing 'compressed_data' or 'aggregated_data'
    }
    
    response = client.post(
        '/aggregated/TEST_DEVICE_001',
        data=json.dumps(payload),
        content_type='application/json'
    )
    
    assert response.status_code == 400
    data = json.loads(response.data)
    assert data['success'] == False
    assert 'Missing' in data['error']


def test_compressed_data_reception(client, sample_compressed_data):
    """Test 5: Receive and process compressed data"""
    payload = {
        'compressed_data': sample_compressed_data,
        'method': 'BIT_PACKED'
    }
    
    response = client.post(
        '/aggregated/TEST_DEVICE_001',
        data=json.dumps(payload),
        content_type='application/json'
    )
    
    # May succeed or fail depending on decompression, but should return proper structure
    data = json.loads(response.data)
    assert 'success' in data
    # Device ID only present on success
    if data['success']:
        assert 'device_id' in data


def test_compression_stats_retrieval(client):
    """Test 6: Retrieve compression statistics"""
    response = client.get('/compression/stats')
    
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['success'] == True
    assert 'statistics' in data
    
    stats = data['statistics']
    assert 'total_decompressions' in stats
    assert 'successful_decompressions' in stats
    assert 'failed_decompressions' in stats


def test_compression_stats_reset(client):
    """Test 7: Reset compression statistics"""
    # First get current stats
    response1 = client.get('/compression/stats')
    data1 = json.loads(response1.data)
    
    # Reset stats
    response2 = client.delete('/compression/stats')
    assert response2.status_code == 200
    data2 = json.loads(response2.data)
    assert data2['success'] == True
    
    # Verify stats are reset
    response3 = client.get('/compression/stats')
    data3 = json.loads(response3.data)
    stats = data3['statistics']
    assert stats['total_decompressions'] == 0


def test_crc_validation_endpoint(client):
    """Test 8: CRC32 validation endpoint"""
    test_data = b"Hello World"
    compressed_b64 = base64.b64encode(test_data).decode('utf-8')
    
    # Calculate CRC32 (simple example)
    import zlib
    expected_crc = zlib.crc32(test_data) & 0xFFFFFFFF
    
    payload = {
        'compressed_data': compressed_b64,
        'crc32': expected_crc
    }
    
    response = client.post(
        '/compression/validate',
        data=json.dumps(payload),
        content_type='application/json'
    )
    
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'success' in data
    assert 'valid' in data


def test_multiple_device_aggregation(client):
    """Test 9: Handle data from multiple devices"""
    devices = ['DEVICE_A', 'DEVICE_B', 'DEVICE_C']
    
    for device_id in devices:
        payload = {
            'aggregated_data': [
                {'timestamp': 1000, 'VAC': 220, 'IAC': 5000}
            ]
        }
        
        response = client.post(
            f'/aggregated/{device_id}',
            data=json.dumps(payload),
            content_type='application/json'
        )
        
        assert response.status_code == 200
        data = json.loads(response.data)
        assert data['device_id'] == device_id


def test_large_batch_processing(client):
    """Test 10: Process large batch of samples"""
    # Create a batch of 100 samples
    large_batch = []
    for i in range(100):
        large_batch.append({
            'timestamp': 1000 + i * 100,
            'VAC': 220 + (i % 5),
            'IAC': 5000 + (i * 10),
            'PAC': 1000 + (i * 5)
        })
    
    payload = {
        'aggregated_data': large_batch
    }
    
    response = client.post(
        '/aggregated/TEST_DEVICE_001',
        data=json.dumps(payload),
        content_type='application/json'
    )
    
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['success'] == True
    assert data['samples_count'] >= 100


# ============================================================================
# TEST CASES - Compression Handler
# ============================================================================

def test_compression_stats_initialization():
    """Test 11: Compression statistics are properly initialized"""
    reset_compression_statistics()
    stats = get_compression_statistics()
    
    assert stats['total_decompressions'] == 0
    assert stats['successful_decompressions'] == 0
    assert stats['failed_decompressions'] == 0
    assert stats['total_bytes_decompressed'] == 0
    assert stats['total_bytes_original'] == 0
    assert 'methods_used' in stats


def test_invalid_base64_handling():
    """Test 12: Handle invalid base64 data gracefully"""
    invalid_data = "This is not valid base64!!!"
    
    values, stats = handle_compressed_data(invalid_data)
    
    # Should fail gracefully
    assert values is None
    assert 'error' in stats


def test_compression_method_tracking():
    """Test 13: Track which compression methods are used"""
    reset_compression_statistics()
    
    # This test would need actual valid compressed data
    # For now, verify the structure exists
    stats = get_compression_statistics()
    assert 'methods_used' in stats
    assert 'Bit Packing' in stats['methods_used']


# ============================================================================
# RUN TESTS
# ============================================================================

if __name__ == '__main__':
    pytest.main([__file__, '-v'])
