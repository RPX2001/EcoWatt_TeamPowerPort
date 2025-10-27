"""
Aggregation routes for EcoWatt Flask server
Handles compressed and aggregated data endpoints
"""

from flask import Blueprint, request, jsonify
import logging
import json
import time

from handlers import (
    handle_compressed_data,
    validate_compression_crc,
    get_compression_statistics,
    reset_compression_statistics,
    handle_aggregated_data
)
from utils.mqtt_utils import publish_mqtt

logger = logging.getLogger(__name__)

# Create blueprint
aggregation_bp = Blueprint('aggregation', __name__)


@aggregation_bp.route('/aggregated/<device_id>', methods=['POST'])
def receive_aggregated_data(device_id: str):
    """Receive and process aggregated/compressed data from device"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        # Check if data contains compressed payload
        if 'compressed_data' in data:
            # Handle compressed data
            compressed_data = data['compressed_data']
            
            decompressed, stats = handle_compressed_data(compressed_data)
            
            if decompressed is not None and stats.get('success'):
                # Publish decompressed data to MQTT
                mqtt_topic = f"ecowatt/data/{device_id}"
                mqtt_payload = {
                    'device_id': device_id,
                    'timestamp': time.time(),
                    'values': decompressed,
                    'sample_count': len(decompressed),
                    'compression_stats': {
                        'method': stats.get('method', 'unknown'),
                        'ratio': stats.get('ratio', 0),
                        'original_size': stats.get('original_size', 0),
                        'compressed_size': stats.get('compressed_size', 0)
                    }
                }
                
                try:
                    publish_mqtt(mqtt_topic, json.dumps(mqtt_payload))
                    logger.info(f"Published {len(decompressed)} decompressed samples to MQTT topic: {mqtt_topic}")
                except Exception as mqtt_error:
                    logger.warning(f"Failed to publish to MQTT: {mqtt_error}")
                    # Don't fail the request if MQTT publish fails
                
                return jsonify({
                    'success': True,
                    'device_id': device_id,
                    'message': 'Compressed data processed and published',
                    'samples_count': len(decompressed) if decompressed else 0,
                    'stats': stats
                }), 200
            else:
                return jsonify({
                    'success': False,
                    'error': stats.get('error', 'Decompression failed')
                }), 400
        
        elif 'aggregated_data' in data:
            # Handle aggregated data (JSON list format)
            aggregated_data = data['aggregated_data']
            
            # Simple handler for JSON list of samples
            if isinstance(aggregated_data, list):
                try:
                    # Process the list of sample dictionaries
                    sample_count = len(aggregated_data)
                    
                    # Publish aggregated data to MQTT
                    mqtt_topic = f"ecowatt/data/{device_id}"
                    mqtt_payload = {
                        'device_id': device_id,
                        'timestamp': time.time(),
                        'samples': aggregated_data,
                        'sample_count': sample_count,
                        'type': 'aggregated'
                    }
                    
                    try:
                        publish_mqtt(mqtt_topic, json.dumps(mqtt_payload))
                        logger.info(f"Published {sample_count} aggregated samples to MQTT topic: {mqtt_topic}")
                    except Exception as mqtt_error:
                        logger.warning(f"Failed to publish to MQTT: {mqtt_error}")
                        # Don't fail the request if MQTT publish fails
                    
                    return jsonify({
                        'success': True,
                        'device_id': device_id,
                        'message': 'Aggregated data processed and published',
                        'samples_count': sample_count
                    }), 200
                except Exception as e:
                    return jsonify({
                        'success': False,
                        'error': f'Failed to process aggregated data: {str(e)}'
                    }), 400
            else:
                return jsonify({
                    'success': False,
                    'error': 'aggregated_data must be a list'
                }), 400
        
        else:
            return jsonify({
                'success': False,
                'error': 'Missing compressed_data or aggregated_data field'
            }), 400
        
    except Exception as e:
        logger.error(f"Error processing aggregated data for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@aggregation_bp.route('/compression/validate', methods=['POST'])
def validate_crc():
    """Validate CRC32 of compressed data"""
    try:
        if not request.is_json:
            return jsonify({
                'success': False,
                'error': 'Content-Type must be application/json'
            }), 400
        
        data = request.get_json()
        
        compressed_data = data.get('compressed_data')
        expected_crc = data.get('crc32')
        
        if not compressed_data or expected_crc is None:
            return jsonify({
                'success': False,
                'error': 'Missing compressed_data or crc32 field'
            }), 400
        
        is_valid = validate_compression_crc(compressed_data, expected_crc)
        
        return jsonify({
            'success': True,
            'valid': is_valid
        }), 200
        
    except Exception as e:
        logger.error(f"Error validating CRC: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@aggregation_bp.route('/compression/stats', methods=['GET'])
def get_compression_stats():
    """Get compression statistics"""
    try:
        stats = get_compression_statistics()
        
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting compression stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@aggregation_bp.route('/compression/stats', methods=['DELETE'])
def reset_compression_stats():
    """Reset compression statistics"""
    try:
        reset_compression_statistics()
        
        return jsonify({
            'success': True,
            'message': 'Compression statistics reset'
        }), 200
        
    except Exception as e:
        logger.error(f"Error resetting compression stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
