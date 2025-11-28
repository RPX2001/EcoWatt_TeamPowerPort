"""
General routes for EcoWatt Flask server
Handles health check, status, and general information endpoints
"""

from flask import Blueprint, jsonify
import logging
from datetime import datetime

from handlers import (
    get_compression_statistics,
    get_security_stats,
    get_ota_stats,
    get_command_stats,
    get_fault_statistics
)
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
general_bp = Blueprint('general', __name__)


@general_bp.route('/', methods=['GET'])
def index():
    """Root endpoint - API information"""
    return jsonify({
        'service': 'EcoWatt Flask Server',
        'version': '1.0.0',
        'status': 'running',
        'timestamp': datetime.now().isoformat(),
        'endpoints': {
            'health': '/health',
            'stats': '/stats',
            'diagnostics': '/diagnostics',
            'security': '/security',
            'aggregation': '/aggregated',
            'ota': '/ota',
            'commands': '/commands',
            'fault': '/fault'
        }
    }), 200


@general_bp.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    try:
        log_success(logger, "Health check passed")
        return jsonify({
            'status': 'healthy',
            'timestamp': datetime.now().isoformat(),
            'service': 'EcoWatt Flask Server'
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Health check failed: {e}")
        return jsonify({
            'status': 'unhealthy',
            'error': str(e)
        }), 503


@general_bp.route('/stats', methods=['GET'])
def get_all_statistics():
    """Get all system statistics"""
    try:
        logger.debug("Gathering system statistics from all modules")
        stats = {
            'timestamp': datetime.now().isoformat(),
            'compression': get_compression_statistics(),
            'security': get_security_stats(),
            'ota': get_ota_stats(),
            'commands': get_command_stats(),
            'fault': get_fault_statistics()
        }
        
        log_success(logger, "Retrieved all system statistics")
        return jsonify({
            'success': True,
            'statistics': stats
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting all statistics: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@general_bp.route('/status', methods=['GET'])
def get_system_status():
    """Get overall system status"""
    try:
        logger.debug("Gathering module status information")
        # Gather system status information
        compression_stats = get_compression_statistics()
        security_stats = get_security_stats()
        ota_stats = get_ota_stats()
        command_stats = get_command_stats()
        
        status = {
            'timestamp': datetime.now().isoformat(),
            'system': 'operational',
            'modules': {
                'compression': {
                    'status': 'active',
                    'total_decompressions': compression_stats.get('total_decompressions', 0)
                },
                'security': {
                    'status': 'active',
                    'total_validations': security_stats.get('total_validations', 0),
                    'success_rate': security_stats.get('success_rate', 0)
                },
                'ota': {
                    'status': 'active',
                    'active_sessions': ota_stats.get('active_sessions', 0),
                    'success_rate': ota_stats.get('success_rate', 0)
                },
                'commands': {
                    'status': 'active',
                    'pending_commands': command_stats.get('pending_commands', 0),
                    'success_rate': command_stats.get('success_rate', 0)
                }
            }
        }
        
        log_success(logger, "System status retrieved - all modules operational")
        return jsonify({
            'success': True,
            'status': status
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting system status: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@general_bp.route('/ping', methods=['GET'])
def ping():
    """Simple ping endpoint"""
    return jsonify({
        'response': 'pong',
        'timestamp': datetime.now().isoformat()
    }), 200


@general_bp.route('/integration/test/sync', methods=['POST'])
def integration_test_sync():
    """
    Integration test synchronization endpoint
    Used by ESP32 integration tests to report test progress
    """
    from flask import request
    
    try:
        data = request.get_json()
        
        if not data:
            logger.warning("✗ Test sync called without data")
            return jsonify({
                'success': False,
                'error': 'No data provided'
            }), 400
        
        test_number = data.get('test_number')
        test_name = data.get('test_name')
        status = data.get('status')
        result = data.get('result')
        
        if result == 'PASS':
            log_success(logger, f"Test {test_number}: {test_name} - {status}")
        else:
            logger.error(f"✗ Test {test_number}: {test_name} - {status} ({result})")
        
        return jsonify({
            'success': True,
            'message': 'Test sync received',
            'test_number': test_number,
            'test_name': test_name,
            'timestamp': datetime.now().isoformat()
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error in test sync: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@general_bp.route('/config', methods=['GET'])
def get_device_config():
    """
    Get device configuration
    Used for remote configuration updates
    """
    from flask import request
    
    try:
        device_id = request.args.get('device_id', 'default')
        
        # Return default configuration
        # In production, this would be stored in database per device
        config = {
            'poll_frequency': 30,      # seconds
            'upload_frequency': 300,   # seconds (5 minutes)
            'register_list': [40001, 40002, 40003, 40004, 40005],
            'compression_enabled': True,
            'security_enabled': True
        }
        
        log_success(logger, f"Configuration sent to device: {device_id}")
        
        return jsonify({
            'success': True,
            'device_id': device_id,
            'config': config,
            'timestamp': datetime.now().isoformat()
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting config: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
