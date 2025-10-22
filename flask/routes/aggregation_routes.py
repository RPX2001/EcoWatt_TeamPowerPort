"""
Aggregation routes for EcoWatt Flask server
Handles compressed and aggregated data endpoints
"""

from flask import Blueprint, request, jsonify
import logging

from handlers import (
    handle_compressed_data,
    validate_compression_crc,
    get_compression_statistics,
    reset_compression_statistics,
    handle_aggregated_data
)

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
            method = data.get('method', 'auto')
            
            success, decompressed, error = handle_compressed_data(
                compressed_data, 
                method
            )
            
            if success:
                return jsonify({
                    'success': True,
                    'device_id': device_id,
                    'message': 'Compressed data processed',
                    'samples_count': len(decompressed) if decompressed else 0
                }), 200
            else:
                return jsonify({
                    'success': False,
                    'error': error or 'Decompression failed'
                }), 400
        
        elif 'aggregated_data' in data:
            # Handle aggregated data
            aggregated_data = data['aggregated_data']
            
            success, samples, error = handle_aggregated_data(aggregated_data)
            
            if success:
                return jsonify({
                    'success': True,
                    'device_id': device_id,
                    'message': 'Aggregated data processed',
                    'samples_count': len(samples) if samples else 0
                }), 200
            else:
                return jsonify({
                    'success': False,
                    'error': error or 'Aggregation processing failed'
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
        success = reset_compression_statistics()
        
        if success:
            return jsonify({
                'success': True,
                'message': 'Compression statistics reset'
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'Failed to reset statistics'
            }), 500
        
    except Exception as e:
        logger.error(f"Error resetting compression stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
