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
        logger.error(f"Error preparing firmware: {e}")
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
        logger.error(f"Error generating keys: {e}")
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
        logger.error(f"Error running benchmark: {e}")
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
            }
        }
    }), 200
