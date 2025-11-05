"""
Power Management Routes
Handles power management configuration and energy reporting
Based on: Milestone 5 - Power Management Specification
"""

from flask import Blueprint, jsonify, request
import logging
import time
from typing import Dict, List, Optional
from datetime import datetime, timedelta
from database import Database, convert_utc_to_local
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)

# Create blueprint
power_bp = Blueprint('power', __name__)

# In-memory power configuration storage (syncs with device_configs)
# This stores: {device_id: {"enabled": bool, "techniques": int, "energy_poll_freq": int}}
power_configs: Dict[str, dict] = {}


def get_default_power_config():
    """Get default power configuration for new devices"""
    return {
        'enabled': False,                # Power management disabled by default
        'techniques': 0x01,              # WiFi modem sleep only (0x01)
        'energy_poll_freq': 300000       # 5 minutes in milliseconds
    }


@power_bp.route('/power/<device_id>', methods=['GET'])
def get_power_config(device_id):
    """
    Get current power management configuration for a device
    
    Returns:
        {
            "device_id": "ESP32_TEST_DEVICE",
            "power_management": {
                "enabled": false,
                "techniques": "0x01",
                "techniques_list": ["wifi_modem_sleep"],
                "energy_poll_freq": 300000
            }
        }
    """
    try:
        # Initialize default config if device not found
        if device_id not in power_configs:
            power_configs[device_id] = get_default_power_config()
        
        config = power_configs[device_id]
        
        # Decode techniques bitmask into list of active techniques
        techniques_list = []
        techniques_val = config['techniques']
        if techniques_val & 0x01:
            techniques_list.append('wifi_modem_sleep')
        if techniques_val & 0x02:
            techniques_list.append('cpu_freq_scaling')
        if techniques_val & 0x04:
            techniques_list.append('light_sleep')
        if techniques_val & 0x08:
            techniques_list.append('peripheral_gating')
        
        return jsonify({
            'device_id': device_id,
            'power_management': {
                'enabled': config['enabled'],
                'techniques': f"0x{techniques_val:02X}",
                'techniques_list': techniques_list,
                'energy_poll_freq': config['energy_poll_freq']
            }
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting power config for {device_id}: {e}")
        return jsonify({'error': 'Failed to retrieve power configuration'}), 500


@power_bp.route('/power/<device_id>', methods=['POST'])
def update_power_config(device_id):
    """
    Update power management configuration for a device
    
    Request Body:
        {
            "enabled": true,
            "techniques": "0x05",  // or integer 5 (WiFi modem + light sleep)
            "energy_poll_freq": 300000
        }
    
    Returns:
        {
            "status": "success",
            "device_id": "ESP32_TEST_DEVICE",
            "updated_config": {...}
        }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        # Initialize if not exists
        if device_id not in power_configs:
            power_configs[device_id] = get_default_power_config()
        
        current_config = power_configs[device_id].copy()
        
        # Update enabled status
        if 'enabled' in data:
            if not isinstance(data['enabled'], bool):
                return jsonify({'error': 'enabled must be a boolean'}), 400
            current_config['enabled'] = data['enabled']
        
        # Update techniques bitmask
        if 'techniques' in data:
            techniques = data['techniques']
            # Handle string format "0x05" or integer 5
            if isinstance(techniques, str):
                if techniques.startswith('0x'):
                    techniques = int(techniques, 16)
                else:
                    techniques = int(techniques)
            elif not isinstance(techniques, int):
                return jsonify({'error': 'techniques must be integer or hex string'}), 400
            
            # Validate range (0x00 to 0x0F)
            if techniques < 0x00 or techniques > 0x0F:
                return jsonify({'error': 'techniques must be in range 0x00-0x0F'}), 400
            
            current_config['techniques'] = techniques
        
        # Update energy poll frequency
        if 'energy_poll_freq' in data:
            freq = data['energy_poll_freq']
            if not isinstance(freq, int) or freq < 60000:  # Minimum 1 minute
                return jsonify({'error': 'energy_poll_freq must be integer >= 60000 (1 minute)'}), 400
            current_config['energy_poll_freq'] = freq
        
        # Save updated config
        power_configs[device_id] = current_config
        
        logger.info(f"Updated power config for {device_id}: enabled={current_config['enabled']}, "
                   f"techniques=0x{current_config['techniques']:02X}, "
                   f"freq={current_config['energy_poll_freq']}ms")
        
        # Decode techniques for response
        techniques_list = []
        if current_config['techniques'] & 0x01:
            techniques_list.append('wifi_modem_sleep')
        if current_config['techniques'] & 0x02:
            techniques_list.append('cpu_freq_scaling')
        if current_config['techniques'] & 0x04:
            techniques_list.append('light_sleep')
        if current_config['techniques'] & 0x08:
            techniques_list.append('peripheral_gating')
        
        return jsonify({
            'status': 'success',
            'device_id': device_id,
            'updated_config': {
                'enabled': current_config['enabled'],
                'techniques': f"0x{current_config['techniques']:02X}",
                'techniques_list': techniques_list,
                'energy_poll_freq': current_config['energy_poll_freq']
            }
        }), 200
        
    except ValueError as e:
        logger.error(f"Invalid value in power config update for {device_id}: {e}")
        return jsonify({'error': f'Invalid value: {str(e)}'}), 400
    except Exception as e:
        logger.error(f"✗ Error updating power config for {device_id}: {e}")
        return jsonify({'error': 'Failed to update power configuration'}), 500


@power_bp.route('/power/energy/<device_id>', methods=['POST'])
def receive_energy_report(device_id):
    """
    Receive energy report from ESP32 device
    
    Request Body:
        {
            "device_id": "ESP32_TEST_DEVICE",
            "timestamp": 1730304000000,
            "power_management": {
                "enabled": true,
                "techniques": "0x05",
                "avg_current_ma": 123.45,
                "energy_saved_mah": 67.89,
                "uptime_ms": 3600000,
                "high_perf_ms": 900000,
                "normal_ms": 1800000,
                "low_power_ms": 600000,
                "sleep_ms": 300000
            }
        }
    
    Returns:
        {"status": "success", "report_id": 123}
    """
    logger.info(f"========== ENERGY REPORT RECEIVED ==========")
    logger.info(f"Device: {device_id}")
    logger.info(f"Endpoint: POST /power/energy/{device_id}")
    
    try:
        data = request.get_json()
        logger.info(f"Raw data: {data}")
        
        if not data:
            logger.error(f"No data provided in request")
            return jsonify({'error': 'No data provided'}), 400
        
        # Validate required fields
        if 'power_management' not in data:
            return jsonify({'error': 'Missing power_management data'}), 400
        
        pm_data = data['power_management']
        required_fields = ['enabled', 'techniques', 'avg_current_ma', 'energy_saved_mah', 'uptime_ms']
        
        for field in required_fields:
            if field not in pm_data:
                return jsonify({'error': f'Missing required field: power_management.{field}'}), 400
        
        # Parse techniques
        techniques = pm_data['techniques']
        if isinstance(techniques, str):
            if techniques.startswith('0x'):
                techniques = int(techniques, 16)
            else:
                techniques = int(techniques)
        
        # Save to database
        report_id = Database.save_power_report(
            device_id=device_id,
            timestamp=data.get('timestamp', int(time.time() * 1000)),
            enabled=pm_data['enabled'],
            techniques=techniques,
            avg_current_ma=pm_data['avg_current_ma'],
            energy_saved_mah=pm_data['energy_saved_mah'],
            uptime_ms=pm_data['uptime_ms'],
            high_perf_ms=pm_data.get('high_perf_ms', 0),
            normal_ms=pm_data.get('normal_ms', 0),
            low_power_ms=pm_data.get('low_power_ms', 0),
            sleep_ms=pm_data.get('sleep_ms', 0)
        )
        
        logger.info(f"Received energy report from {device_id}: "
                   f"avg_current={pm_data['avg_current_ma']:.2f}mA, "
                   f"energy_saved={pm_data['energy_saved_mah']:.2f}mAh")
        
        return jsonify({
            'status': 'success',
            'report_id': report_id
        }), 201
        
    except ValueError as e:
        logger.error(f"Invalid value in energy report from {device_id}: {e}")
        return jsonify({'error': f'Invalid value: {str(e)}'}), 400
    except Exception as e:
        logger.error(f"✗ Error receiving energy report from {device_id}: {e}")
        return jsonify({'error': 'Failed to save energy report'}), 500


@power_bp.route('/power/energy/<device_id>', methods=['GET'])
def get_energy_history(device_id):
    """
    Get historical energy reports for a device
    
    Query Parameters:
        period: Time period (1h, 24h, 7d, 30d) - default: 24h
        limit: Maximum number of records - default: 100
    
    Returns:
        {
            "device_id": "ESP32_TEST_DEVICE",
            "period": "24h",
            "count": 48,
            "reports": [
                {
                    "timestamp": "2025-10-30T12:00:00+05:30",
                    "enabled": true,
                    "techniques": "0x05",
                    "techniques_list": ["wifi_modem_sleep", "light_sleep"],
                    "avg_current_ma": 123.45,
                    "energy_saved_mah": 67.89,
                    "uptime_ms": 3600000,
                    "time_distribution": {
                        "high_perf_ms": 900000,
                        "normal_ms": 1800000,
                        "low_power_ms": 600000,
                        "sleep_ms": 300000
                    }
                },
                ...
            ]
        }
    """
    try:
        # Parse query parameters
        period = request.args.get('period', '24h')
        limit = request.args.get('limit', 100, type=int)
        
        # Validate period (allow 'all' for all data)
        valid_periods = ['1h', '24h', '7d', '30d', 'all']
        if period not in valid_periods:
            return jsonify({'error': f'Invalid period. Must be one of: {", ".join(valid_periods)}'}), 400
        
        # Fetch from database
        reports_raw = Database.get_power_reports(device_id, period, limit)
        
        # Format reports
        reports = []
        for report in reports_raw:
            # Decode techniques
            techniques = report['techniques']
            techniques_list = []
            if techniques & 0x01:
                techniques_list.append('wifi_modem_sleep')
            if techniques & 0x02:
                techniques_list.append('cpu_freq_scaling')
            if techniques & 0x04:
                techniques_list.append('light_sleep')
            if techniques & 0x08:
                techniques_list.append('peripheral_gating')
            
            reports.append({
                'timestamp': convert_utc_to_local(report['timestamp']),
                'enabled': bool(report['enabled']),
                'techniques': f"0x{techniques:02X}",
                'techniques_list': techniques_list,
                'avg_current_ma': report['avg_current_ma'],
                'energy_saved_mah': report['energy_saved_mah'],
                'uptime_ms': report['uptime_ms'],
                'time_distribution': {
                    'high_perf_ms': report['high_perf_ms'],
                    'normal_ms': report['normal_ms'],
                    'low_power_ms': report['low_power_ms'],
                    'sleep_ms': report['sleep_ms']
                }
            })
        
        return jsonify({
            'device_id': device_id,
            'period': period,
            'count': len(reports),
            'reports': reports
        }), 200
        
    except Exception as e:
        logger.error(f"✗ Error getting energy history for {device_id}: {e}")
        return jsonify({'error': 'Failed to retrieve energy history'}), 500
