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
        
        # Get JSON with no size limit (handled by MAX_CONTENT_LENGTH in app config)
        data = request.get_json(force=True, cache=False)
        
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
            # Note: decrypted_payload can be either str (uncompressed JSON) or bytes (compressed binary)
            try:
                if isinstance(decrypted_payload, bytes):
                    # This shouldn't happen for aggregated endpoint - compressed binary should be inside JSON
                    logger.error(f"Unexpected binary payload for {device_id}")
                    return jsonify({
                        'success': False,
                        'error': 'Unexpected binary payload format'
                    }), 400
                else:
                    # Uncompressed JSON payload (may contain compressed_data field inside)
                    data = json.loads(decrypted_payload)
                    logger.info(f"Secured payload validated for {device_id}")
            except Exception as e:
                logger.error(f"Failed to parse decrypted payload: {e}")
                return jsonify({
                    'success': False,
                    'error': 'Failed to parse decrypted payload'
                }), 400
        
        # Auto-register device if not already registered
        from routes.device_routes import devices_registry
        if device_id not in devices_registry:
            devices_registry[device_id] = {
                'device_name': f'EcoWatt {device_id}',
                'location': 'Auto-registered',
                'description': 'Automatically registered on first data upload',
                'status': 'active',
                'firmware_version': 'unknown',
                'registered_at': time.time(),
                'last_seen': time.time()
            }
            logger.info(f"Auto-registered new device: {device_id}")
        else:
            # Update last_seen timestamp
            devices_registry[device_id]['last_seen'] = time.time()
        
        # Check if data contains compressed payload
        if 'compressed_data' in data:
            # Handle compressed data - can be array of packets, single base64, or raw bytes
            compressed_data = data['compressed_data']
            
            # If compressed_data is bytes (from secured compressed payload), convert to base64
            if isinstance(compressed_data, bytes):
                import base64
                compressed_data = base64.b64encode(compressed_data).decode('ascii')
                logger.info(f"Converted {len(data['compressed_data'])} raw bytes to base64: {len(compressed_data)} chars")
            
            # If compressed_data is a list of packets, process each packet
            if isinstance(compressed_data, list):
                all_decompressed = []
                all_stats = []
                
                for idx, packet in enumerate(compressed_data):
                    # Extract compressed_binary from packet
                    if isinstance(packet, dict) and 'compressed_binary' in packet:
                        base64_data = packet['compressed_binary']
                        
                        # Debug: Log the base64 data
                        logger.info(f"Packet {idx+1}: Base64 length={len(base64_data) if isinstance(base64_data, str) else 'not_string'}")
                        logger.info(f"Packet {idx+1}: Base64 data={base64_data[:50] if isinstance(base64_data, str) else type(base64_data)}")
                        
                        # Decompress this packet
                        decompressed, stats = handle_compressed_data(base64_data)
                        
                        if decompressed is not None and stats.get('success'):
                            all_decompressed.extend(decompressed)
                            all_stats.append(stats)
                            logger.info(f"Packet {idx+1}/{len(compressed_data)}: {len(decompressed)} samples decompressed")
                        else:
                            logger.error(f"Packet {idx+1} decompression failed: {stats.get('error')}")
                    else:
                        logger.error(f"Packet {idx+1} missing compressed_binary field")
                
                if all_decompressed:
                    # Publish decompressed data to MQTT
                    mqtt_topic = f"ecowatt/data/{device_id}"
                    current_timestamp = time.time()
                    
                    # Extract register layout from first packet's metadata
                    register_layout = []
                    if isinstance(compressed_data, list) and len(compressed_data) > 0 and isinstance(compressed_data[0], dict):
                        metadata = compressed_data[0].get('decompression_metadata', {})
                        register_layout = metadata.get('register_layout', [])
                    
                    # Calculate samples based on register count
                    registers_per_sample = len(register_layout) if register_layout else 10
                    total_sample_count = len(all_decompressed) // registers_per_sample
                    
                    mqtt_payload = {
                        'device_id': device_id,
                        'timestamp': current_timestamp,
                        'values': all_decompressed,
                        'sample_count': total_sample_count,
                        'registers_per_sample': registers_per_sample,
                        'register_layout': register_layout,
                        'packet_count': len(all_stats),
                        'compression_stats': {
                            'methods': [s.get('method', 'unknown') for s in all_stats],
                            'total_samples': total_sample_count,
                            'packets_processed': len(all_stats)
                        }
                    }
                    
                    # Store latest data for dashboard (both memory and database)
                    from utils.data_utils import store_device_latest_data
                    
                    # Calculate average compression ratio
                    avg_compression_ratio = sum(s.get('compression_ratio', 0) for s in all_stats) / len(all_stats) if all_stats else None
                    
                    # Get most common compression method
                    compression_method = all_stats[0].get('method', 'unknown') if all_stats else None
                    
                    # Extract first packet's timestamp for proper timestamp calculation
                    first_packet_timestamp = compressed_data[0].get('decompression_metadata', {}).get('timestamp', current_timestamp * 1000) if isinstance(compressed_data, list) and len(compressed_data) > 0 and isinstance(compressed_data[0], dict) else current_timestamp * 1000
                    
                    store_device_latest_data(
                        device_id=device_id, 
                        values=all_decompressed, 
                        timestamp=first_packet_timestamp,  # Use ESP32's timestamp in milliseconds
                        compression_method=compression_method,
                        compression_ratio=avg_compression_ratio,
                        sample_count=total_sample_count,
                        register_layout=register_layout  # Pass the actual registers that were polled
                    )
                    
                    try:
                        publish_mqtt(mqtt_topic, json.dumps(mqtt_payload))
                        logger.info(f"Published {len(all_decompressed)} decompressed samples from {len(all_stats)} packets to MQTT")
                    except Exception as mqtt_error:
                        logger.warning(f"Failed to publish to MQTT: {mqtt_error}")
                    
                    return jsonify({
                        'success': True,
                        'device_id': device_id,
                        'message': 'Compressed data processed and published',
                        'samples_count': len(all_decompressed),
                        'packets_processed': len(all_stats),
                        'stats': all_stats
                    }), 200
                else:
                    return jsonify({
                        'success': False,
                        'error': 'No packets successfully decompressed'
                    }), 400
            
            else:
                # Single base64 string (legacy format)
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
                        logger.info(f"Published {len(decompressed)} decompressed samples to MQTT")
                    except Exception as mqtt_error:
                        logger.warning(f"Failed to publish to MQTT: {mqtt_error}")
                    
                    return jsonify({
                        'success': True,
                        'device_id': device_id,
                        'message': 'Compressed data processed and published',
                        'samples_count': len(decompressed),
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


@aggregation_bp.route('/aggregation/historical/<device_id>', methods=['GET'])
def get_historical_data(device_id: str):
    """
    Get historical aggregated data for a device from database
    
    Args:
        device_id: Device identifier
        
    Query parameters:
        start_time: Start timestamp (ISO format or unix timestamp)
        end_time: End timestamp (ISO format or unix timestamp)
        limit: Maximum number of records (default: 1000, max: 10000)
        
    Returns:
        JSON with historical sensor data from database
    """
    try:
        from database import Database
        from datetime import datetime, timedelta
        
        # Parse query parameters
        start_time_str = request.args.get('start_time')
        end_time_str = request.args.get('end_time')
        limit = min(int(request.args.get('limit', 1000)), 10000)  # Cap at 10k records
        
        # Convert timestamps if provided
        start_time = None
        end_time = None
        
        if start_time_str:
            try:
                # Try parsing as unix timestamp
                start_time = datetime.fromtimestamp(float(start_time_str))
            except ValueError:
                # Try parsing as ISO format
                start_time = datetime.fromisoformat(start_time_str.replace('Z', '+00:00'))
        
        if end_time_str:
            try:
                end_time = datetime.fromtimestamp(float(end_time_str))
            except ValueError:
                end_time = datetime.fromisoformat(end_time_str.replace('Z', '+00:00'))
        
        # Query database for historical data
        historical_data = Database.get_historical_sensor_data(
            device_id=device_id,
            start_time=start_time,
            end_time=end_time,
            limit=limit
        )
        
        # Format response
        data_points = []
        for record in historical_data:
            data_point = {
                'timestamp': record['timestamp'],
                'registers': record['register_data'],
                'compression_method': record['compression_method'],
                'compression_ratio': record['compression_ratio']
            }
            data_points.append(data_point)
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'data': data_points,
            'count': len(data_points),
            'start_time': start_time.isoformat() if start_time else None,
            'end_time': end_time.isoformat() if end_time else None,
            'limit': limit
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting historical data for {device_id}: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


# Alias route for backward compatibility
@aggregation_bp.route('/aggregation/latest/<device_id>', methods=['GET'])
@aggregation_bp.route('/latest/<device_id>', methods=['GET'])
def get_latest_data(device_id: str):
    """
    Get latest sensor readings for a device with register mapping
    
    Args:
        device_id: Device identifier
        
    Returns:
        JSON with latest register values
    """
    try:
        # Import the latest data cache from utils
        from utils.data_utils import get_device_latest_data, REGISTER_METADATA
        
        latest_data = get_device_latest_data(device_id)
        
        if latest_data:
            return jsonify({
                'success': True,
                'device_id': device_id,
                'timestamp': latest_data.get('timestamp'),
                'registers': latest_data.get('registers', {}),
                'raw_values': latest_data.get('values', []),
                'metadata': REGISTER_METADATA
            }), 200
        else:
            return jsonify({
                'success': False,
                'error': 'No data available for this device'
            }), 404
            
    except Exception as e:
        logger.error(f"Error getting latest data for {device_id}: {e}")
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

