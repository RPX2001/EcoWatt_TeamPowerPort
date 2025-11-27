"""
Utilities routes for EcoWatt Flask server
Handles utility tools like firmware preparation, key generation, and benchmarking
"""

from flask import Blueprint, request, jsonify, send_file
import logging
import subprocess
import os
import json
import tempfile
from datetime import datetime
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
utilities_bp = Blueprint('utilities', __name__)

# Get the flask directory path
FLASK_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS_DIR = os.path.join(FLASK_DIR, 'scripts')


@utilities_bp.route('/utilities/firmware/prepare', methods=['POST'])
def prepare_firmware():
    """
    Prepare firmware file with manifest generation
    Expects: firmware file upload, version, algorithm
    """
    try:
        # Check if file is present
        if 'firmware' not in request.files:
            return jsonify({
                'success': False,
                'error': 'No firmware file provided'
            }), 400

        firmware_file = request.files['firmware']
        version = request.form.get('version', '1.0.0')
        algorithm = request.form.get('algorithm', 'zlib')

        if firmware_file.filename == '':
            return jsonify({
                'success': False,
                'error': 'No file selected'
            }), 400

        # Create temporary directory for processing
        with tempfile.TemporaryDirectory() as temp_dir:
            # Save uploaded file
            firmware_path = os.path.join(temp_dir, firmware_file.filename)
            firmware_file.save(firmware_path)

            # Prepare firmware using the script
            prepare_script = os.path.join(SCRIPTS_DIR, 'prepare_firmware.py')
            output_dir = os.path.join(temp_dir, 'output')
            os.makedirs(output_dir, exist_ok=True)

            # Run prepare_firmware.py
            cmd = [
                'python3',
                prepare_script,
                firmware_path,
                '--version', version,
                '--algorithm', algorithm,
                '--output', output_dir
            ]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                logger.error(f"Firmware preparation failed: {result.stderr}")
                return jsonify({
                    'success': False,
                    'error': f'Preparation failed: {result.stderr}'
                }), 500

            # Read generated manifest
            manifest_path = os.path.join(output_dir, f'firmware_{version}_manifest.json')
            if os.path.exists(manifest_path):
                with open(manifest_path, 'r') as f:
                    manifest = json.load(f)
            else:
                manifest = {}

            # Get prepared firmware stats
            prepared_files = os.listdir(output_dir)
            
            return jsonify({
                'success': True,
                'message': 'Firmware prepared successfully',
                'manifest': manifest,
                'files': prepared_files,
                'output': result.stdout
            }), 200

    except subprocess.TimeoutExpired:
        logger.error("Firmware preparation timed out")
        return jsonify({
            'success': False,
            'error': 'Operation timed out'
        }), 504
    except Exception as e:
        logger.error(f"✗ Error preparing firmware: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@utilities_bp.route('/utilities/keys/generate', methods=['POST'])
def generate_keys():
    """
    Generate cryptographic keys
    Expects: key_type (PSK/HMAC), format (c_header/pem/binary)
    """
    try:
        data = request.get_json() or {}
        key_type = data.get('key_type', 'PSK')
        key_format = data.get('format', 'c_header')
        key_size = data.get('key_size', 32)

        # Run generate_keys.py script
        generate_script = os.path.join(SCRIPTS_DIR, 'generate_keys.py')
        
        # Create temp directory for output
        with tempfile.TemporaryDirectory() as temp_dir:
            cmd = [
                'python3',
                generate_script,
                '--type', key_type.lower(),
                '--format', key_format,
                '--size', str(key_size),
                '--output', temp_dir
            ]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode != 0:
                logger.error(f"Key generation failed: {result.stderr}")
                return jsonify({
                    'success': False,
                    'error': f'Generation failed: {result.stderr}'
                }), 500

            # Read generated keys
            keys_data = {}
            for filename in os.listdir(temp_dir):
                filepath = os.path.join(temp_dir, filename)
                if os.path.isfile(filepath):
                    with open(filepath, 'r') as f:
                        keys_data[filename] = f.read()

            return jsonify({
                'success': True,
                'message': 'Keys generated successfully',
                'keys': keys_data,
                'output': result.stdout,
                'timestamp': datetime.now().isoformat()
            }), 200

    except subprocess.TimeoutExpired:
        logger.error("Key generation timed out")
        return jsonify({
            'success': False,
            'error': 'Operation timed out'
        }), 504
    except Exception as e:
        logger.error(f"✗ Error generating keys: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@utilities_bp.route('/utilities/compression/benchmark', methods=['POST'])
def benchmark_compression():
    """
    Run compression algorithm benchmark
    Compares different compression algorithms
    """
    try:
        data = request.get_json() or {}
        test_data_size = data.get('data_size', 1024)  # Default 1KB
        iterations = data.get('iterations', 100)

        # Run benchmark_compression.py script
        benchmark_script = os.path.join(SCRIPTS_DIR, 'benchmark_compression.py')
        
        cmd = [
            'python3',
            benchmark_script,
            '--size', str(test_data_size),
            '--iterations', str(iterations)
        ]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60
        )

        if result.returncode != 0:
            logger.error(f"Benchmark failed: {result.stderr}")
            return jsonify({
                'success': False,
                'error': f'Benchmark failed: {result.stderr}'
            }), 500

        # Parse benchmark results from output
        # Assuming the script outputs JSON results
        try:
            benchmark_results = json.loads(result.stdout)
        except json.JSONDecodeError:
            # If not JSON, return raw output
            benchmark_results = {
                'raw_output': result.stdout
            }

        return jsonify({
            'success': True,
            'message': 'Benchmark completed successfully',
            'results': benchmark_results,
            'parameters': {
                'data_size': test_data_size,
                'iterations': iterations
            },
            'timestamp': datetime.now().isoformat()
        }), 200

    except subprocess.TimeoutExpired:
        logger.error("Benchmark timed out")
        return jsonify({
            'success': False,
            'error': 'Operation timed out'
        }), 504
    except Exception as e:
        logger.error(f"✗ Error running benchmark: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@utilities_bp.route('/utilities/info', methods=['GET'])
def utilities_info():
    """Get information about available utilities"""
    return jsonify({
        'success': True,
        'utilities': {
            'firmware_preparation': {
                'endpoint': '/utilities/firmware/prepare',
                'method': 'POST',
                'description': 'Prepare firmware with manifest generation',
                'parameters': ['firmware file', 'version', 'algorithm']
            },
            'key_generation': {
                'endpoint': '/utilities/keys/generate',
                'method': 'POST',
                'description': 'Generate cryptographic keys',
                'parameters': ['key_type', 'format', 'key_size']
            },
            'compression_benchmark': {
                'endpoint': '/utilities/compression/benchmark',
                'method': 'POST',
                'description': 'Run compression algorithm benchmark',
                'parameters': ['data_size', 'iterations']
            },
            'database_cleanup': {
                'endpoint': '/utilities/database/cleanup',
                'method': 'POST',
                'description': 'Manually cleanup old database records',
                'parameters': ['retention_days (optional)']
            },
            'database_stats': {
                'endpoint': '/utilities/database/stats',
                'method': 'GET',
                'description': 'Get database statistics and storage info',
                'parameters': []
            }
        }
    }), 200


@utilities_bp.route('/utilities/database/cleanup', methods=['POST'])
def database_cleanup():
    """
    Manually cleanup old database records
    Useful when unlimited retention is enabled but periodic cleanup is needed
    """
    from database import Database, RETENTION_DAYS
    
    try:
        data = request.get_json() if request.is_json else {}
        
        # Allow override of retention days for manual cleanup
        custom_retention = data.get('retention_days')
        
        if custom_retention:
            # Temporarily override RETENTION_DAYS
            import database
            original_retention = database.RETENTION_DAYS
            database.RETENTION_DAYS = int(custom_retention)
            
            logger.info(f"Manual cleanup with custom retention: {custom_retention} days")
            result = Database.cleanup_old_data()
            
            # Restore original setting
            database.RETENTION_DAYS = original_retention
        else:
            # Use configured RETENTION_DAYS (may be None)
            result = Database.cleanup_old_data()
        
        return jsonify({
            'success': True,
            'message': 'Database cleanup completed',
            'retention_days': custom_retention or RETENTION_DAYS,
            'records_deleted': result
        }), 200
        
    except Exception as e:
        logger.error(f"Database cleanup failed: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@utilities_bp.route('/utilities/database/stats', methods=['GET'])
def database_stats():
    """Get database statistics and configuration"""
    from database import Database, RETENTION_DAYS, DB_PATH
    import os
    
    try:
        # Get database stats
        stats = Database.get_database_stats()
        
        # Get file size
        db_size_bytes = os.path.getsize(DB_PATH) if os.path.exists(DB_PATH) else 0
        db_size_mb = db_size_bytes / (1024 * 1024)
        
        return jsonify({
            'success': True,
            'database_path': str(DB_PATH),
            'database_size_bytes': db_size_bytes,
            'database_size_mb': round(db_size_mb, 2),
            'retention_policy': {
                'enabled': RETENTION_DAYS is not None,
                'retention_days': RETENTION_DAYS,
                'description': f'Data kept for {RETENTION_DAYS} days' if RETENTION_DAYS else 'Unlimited - data kept forever'
            },
            'record_counts': stats
        }), 200
        
    except Exception as e:
        logger.error(f"Failed to get database stats: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@utilities_bp.route('/utilities/logs/server', methods=['GET'])
def get_server_logs():
    """
    Get recent server logs with filtering options
    """
    try:
        # Get query parameters
        limit = request.args.get('limit', default=100, type=int)
        level = request.args.get('level', default='all', type=str)
        search = request.args.get('search', default='', type=str)
        
        # Limit to reasonable values
        limit = min(limit, 1000)
        
        # Get the logs directory
        logs_dir = os.path.join(FLASK_DIR, 'logs')
        current_log_file = os.path.join(logs_dir, 'ecowatt_server.log')
        
        if not os.path.exists(current_log_file):
            return jsonify({
                'success': True,
                'logs': [],
                'count': 0,
                'message': 'No log file found'
            }), 200
        
        # Read the log file
        logs = []
        try:
            with open(current_log_file, 'r', encoding='utf-8') as f:
                lines = f.readlines()
                
            # Process lines from end (most recent first)
            for line in reversed(lines[-limit*2:]):  # Read more than needed for filtering
                line = line.strip()
                if not line:
                    continue
                
                # Parse log level from line
                log_level = 'INFO'
                if 'ERROR' in line or '✗' in line:
                    log_level = 'ERROR'
                elif 'WARNING' in line or '⚠' in line:
                    log_level = 'WARNING'
                elif 'DEBUG' in line:
                    log_level = 'DEBUG'
                elif 'SUCCESS' in line or '✓' in line:
                    log_level = 'SUCCESS'
                
                # Filter by level
                if level != 'all' and log_level.lower() != level.lower():
                    continue
                
                # Filter by search term
                if search and search.lower() not in line.lower():
                    continue
                
                # Extract timestamp if present
                timestamp = None
                try:
                    # Try to parse timestamp from log line
                    # Format: YYYY-MM-DD HH:MM:SS
                    parts = line.split(' - ', 1)
                    if len(parts) >= 2:
                        timestamp_str = parts[0].strip()
                        # Simple timestamp extraction (adjust based on your log format)
                        if len(timestamp_str) > 10:
                            timestamp = timestamp_str
                except:
                    pass
                
                logs.append({
                    'level': log_level,
                    'message': line,
                    'timestamp': timestamp or datetime.now().isoformat()
                })
                
                if len(logs) >= limit:
                    break
        
        except Exception as e:
            logger.error(f"Error reading log file: {e}")
            return jsonify({
                'success': False,
                'error': f'Failed to read log file: {str(e)}'
            }), 500
        
        return jsonify({
            'success': True,
            'logs': logs,
            'count': len(logs),
            'file': current_log_file
        }), 200
        
    except Exception as e:
        logger.error(f"Failed to get server logs: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@utilities_bp.route('/utilities/logs/files', methods=['GET'])
def list_log_files():
    """
    List available log files
    """
    try:
        logs_dir = os.path.join(FLASK_DIR, 'logs')
        
        if not os.path.exists(logs_dir):
            return jsonify({
                'success': True,
                'files': []
            }), 200
        
        log_files = []
        for filename in os.listdir(logs_dir):
            if filename.endswith('.log'):
                filepath = os.path.join(logs_dir, filename)
                file_stat = os.stat(filepath)
                log_files.append({
                    'name': filename,
                    'size': file_stat.st_size,
                    'modified': datetime.fromtimestamp(file_stat.st_mtime).isoformat()
                })
        
        # Sort by modified time (newest first)
        log_files.sort(key=lambda x: x['modified'], reverse=True)
        
        return jsonify({
            'success': True,
            'files': log_files
        }), 200
        
    except Exception as e:
        logger.error(f"Failed to list log files: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
