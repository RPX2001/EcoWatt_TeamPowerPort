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
        
        # Check if data is a secured payload (has nonce, payload, mac fields)
        if 'nonce' in data and 'payload' in data and 'mac' in data:
            # This is a secured payload - validate it first
            from handlers import validate_secured_payload
            
            # Convert dict to JSON string for validation
            secured_payload_str = json.dumps(data)
            success, decrypted_payload, error = validate_secured_payload(device_id, secured_payload_str)
            
            if not success:
                logger.warning(f"Security validation failed for {device_id}: {error}")
                return jsonify({
                    'success': False,
                    'error': f'Security validation failed: {error}'
                }), 401
            
            # Parse the decrypted payload
            try:
                data = json.loads(decrypted_payload)
                logger.info(f"Secured payload validated for {device_id}")
            except Exception as e:
                logger.error(f"Failed to parse decrypted payload: {e}")
                return jsonify({
                    'success': False,
                    'error': 'Failed to parse decrypted payload'
                }), 400
        
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


@aggregation_bp.route('/aggregation/latest/<device_id>', methods=['GET'])
def get_latest_data(device_id: str):
    """
    Get the latest aggregated data for a device
    
    Args:
        device_id: Device identifier
        
    Returns:
        JSON with latest sensor data
    """
    try:
        # In a real implementation, this would query a database
        # For now, return mock data
        data = {
            'device_id': device_id,
            'timestamp': time.time(),
            'voltage': 230.5,
            'current': 4.2,
            'power': 968.1,
            'energy': 12.5,
            'frequency': 50.0,
            'power_factor': 0.95
        }
        
        return jsonify({
            'success': True,
            **data
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting latest data for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@aggregation_bp.route('/aggregation/historical/<device_id>', methods=['GET'])
def get_historical_data(device_id: str):
    """
    Get historical aggregated data for a device
    
    Args:
        device_id: Device identifier
        
    Query parameters:
        start_time: Start timestamp (ISO format or unix timestamp)
        end_time: End timestamp (ISO format or unix timestamp)
        limit: Maximum number of records (default: 100)
        
    Returns:
        JSON with historical sensor data
    """
    try:
        start_time = request.args.get('start_time')
        end_time = request.args.get('end_time')
        limit = int(request.args.get('limit', 100))
        
        # In a real implementation, this would query a database
        # For now, return mock historical data
        import random
        current_time = time.time()
        data_points = []
        
        for i in range(min(limit, 50)):
            timestamp = current_time - (i * 60)  # 1 minute intervals
            data_points.append({
                'timestamp': timestamp,
                'voltage': 230 + random.uniform(-5, 5),
                'current': 4 + random.uniform(-0.5, 0.5),
                'power': 950 + random.uniform(-50, 50),
                'energy': 12 + random.uniform(0, 1),
                'frequency': 50.0,
                'power_factor': 0.95
            })
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'data': data_points,
            'count': len(data_points)
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting historical data for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@aggregation_bp.route('/export/<device_id>/csv', methods=['GET'])
def export_data_csv(device_id: str):
    """
    Export device data as CSV
    
    Args:
        device_id: Device identifier
        
    Query parameters:
        start_time: Start timestamp
        end_time: End timestamp
        
    Returns:
        CSV file download
    """
    try:
        from flask import make_response
        import io
        import csv
        
        # In a real implementation, query database for data
        # For now, create mock CSV data
        output = io.StringIO()
        writer = csv.writer(output)
        
        # Write header
        writer.writerow(['timestamp', 'voltage', 'current', 'power', 'energy', 'frequency', 'power_factor'])
        
        # Write mock data rows
        import random
        current_time = time.time()
        for i in range(50):
            timestamp = current_time - (i * 60)
            writer.writerow([
                timestamp,
                230 + random.uniform(-5, 5),
                4 + random.uniform(-0.5, 0.5),
                950 + random.uniform(-50, 50),
                12 + random.uniform(0, 1),
                50.0,
                0.95
            ])
        
        # Create response
        response = make_response(output.getvalue())
        response.headers['Content-Type'] = 'text/csv'
        response.headers['Content-Disposition'] = f'attachment; filename={device_id}_data.csv'
        
        return response
        
    except Exception as e:
        logger.error(f"Error exporting CSV for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

