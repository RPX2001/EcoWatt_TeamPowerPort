"""
Data utilities for EcoWatt Flask server
Handles serialization, deserialization, and data processing
"""

import struct
import logging
from datetime import datetime
from typing import Dict, List, Optional, Tuple

logger = logging.getLogger(__name__)


def deserialize_aggregated_sample(binary_data: bytes) -> Optional[Dict]:
    """
    Deserialize aggregated sample data from binary format.
    
    Format:
    - Marker: 0xAA (1 byte) - Fixed from 0xAG
    - Sample Count: uint8 (1 byte)
    - Timestamp Start: uint32 (4 bytes)
    - Timestamp End: uint32 (4 bytes)
    - Min values: uint16[10] (20 bytes)
    - Avg values: uint16[10] (20 bytes)
    - Max values: uint16[10] (20 bytes)
    Total: 70 bytes
    
    Args:
        binary_data: Binary data to deserialize
        
    Returns:
        dict: Aggregated sample data or None if invalid
        {
            'is_aggregated': True,
            'sample_count': 5,
            'timestamp_start': 12340,
            'timestamp_end': 12345,
            'min_values': [2400, 170, ...],
            'avg_values': [2405, 172, ...],
            'max_values': [2410, 175, ...]
        }
    """
    try:
        if len(binary_data) < 70:
            logger.warning(f"Aggregated sample data too short: {len(binary_data)} bytes")
            return None
        
        # Check marker (0xAA = 0xAA in hex)
        if binary_data[0] != 0xAA:
            logger.debug(f"Invalid aggregation marker: 0x{binary_data[0]:02X}")
            return None
        
        idx = 1
        
        # Extract header
        sample_count = binary_data[idx]
        idx += 1
        
        # Timestamps (4 bytes each, little-endian)
        timestamp_start = struct.unpack('<I', binary_data[idx:idx+4])[0]
        idx += 4
        timestamp_end = struct.unpack('<I', binary_data[idx:idx+4])[0]
        idx += 4
        
        # Extract min/avg/max arrays (10 values each)
        min_values = []
        avg_values = []
        max_values = []
        
        # Min values (10 registers)
        for i in range(10):
            val = struct.unpack('<H', binary_data[idx:idx+2])[0]
            min_values.append(val)
            idx += 2
        
        # Avg values (10 registers)
        for i in range(10):
            val = struct.unpack('<H', binary_data[idx:idx+2])[0]
            avg_values.append(val)
            idx += 2
        
        # Max values (10 registers)
        for i in range(10):
            val = struct.unpack('<H', binary_data[idx:idx+2])[0]
            max_values.append(val)
            idx += 2
        
        logger.info(f"Deserialized aggregated sample: {sample_count} samples, "
                   f"timestamp range: {timestamp_start}-{timestamp_end}")
        
        return {
            'is_aggregated': True,
            'sample_count': sample_count,
            'timestamp_start': timestamp_start,
            'timestamp_end': timestamp_end,
            'min_values': min_values,
            'avg_values': avg_values,
            'max_values': max_values
        }
    
    except struct.error as e:
        logger.error(f"Struct unpacking error in aggregated sample: {e}")
        return None
    except Exception as e:
        logger.error(f"Error deserializing aggregated sample: {e}")
        return None


def expand_aggregated_to_samples(aggregated_data: Dict) -> Optional[Dict]:
    """
    Expand aggregated data back to individual samples for storage/analysis.
    Uses average values as representative samples.
    
    Args:
        aggregated_data: Aggregated sample dictionary
        
    Returns:
        dict: Expanded sample data for storage
        {
            'type': 'aggregated',
            'sample_count': 5,
            'samples': [[avg1, avg2, ...], ...],
            'aggregation': {
                'min': [2400, 170, ...],
                'avg': [2405, 172, ...],
                'max': [2410, 175, ...]
            }
        }
    """
    try:
        if not aggregated_data or not aggregated_data.get('is_aggregated'):
            logger.warning("Data is not aggregated, cannot expand")
            return None
        
        result = {
            'type': 'aggregated',
            'sample_count': aggregated_data['sample_count'],
            'timestamp_start': aggregated_data['timestamp_start'],
            'timestamp_end': aggregated_data['timestamp_end'],
            'samples': [aggregated_data['avg_values']],  # Use avg as representative
            'aggregation': {
                'min': aggregated_data['min_values'],
                'avg': aggregated_data['avg_values'],
                'max': aggregated_data['max_values']
            },
            'flat_data': aggregated_data['avg_values'],
            'first_sample': aggregated_data['avg_values']
        }
        
        logger.debug(f"Expanded aggregated data: {result['sample_count']} samples")
        return result
    
    except Exception as e:
        logger.error(f"Error expanding aggregated data: {e}")
        return None


def process_register_data(registers: List[str], decompressed_values: List[float]) -> Dict:
    """
    Map decompressed values to register names for easy access
    
    Args:
        registers: List of register names (e.g., ['Voltage', 'Current', ...])
        decompressed_values: List of decompressed numeric values
        
    Returns:
        dict: Mapped register data
        {
            'Voltage': 230.5,
            'Current': 5.2,
            ...
        }
    """
    try:
        if len(registers) != len(decompressed_values):
            logger.warning(f"Register count mismatch: {len(registers)} registers, "
                         f"{len(decompressed_values)} values")
        
        # Create mapping
        mapped_data = {}
        for i, reg_name in enumerate(registers):
            if i < len(decompressed_values):
                mapped_data[reg_name] = decompressed_values[i]
            else:
                mapped_data[reg_name] = None
        
        logger.debug(f"Mapped {len(mapped_data)} registers")
        return mapped_data
    
    except Exception as e:
        logger.error(f"Error processing register data: {e}")
        return {}


def format_timestamp(timestamp: int) -> str:
    """
    Format timestamp to human-readable string
    
    Args:
        timestamp: Unix timestamp (seconds since epoch)
        
    Returns:
        str: Formatted timestamp string
    """
    try:
        dt = datetime.fromtimestamp(timestamp)
        return dt.strftime('%Y-%m-%d %H:%M:%S')
    except Exception as e:
        logger.error(f"Error formatting timestamp: {e}")
        return str(timestamp)


def calculate_statistics(values: List[float]) -> Dict:
    """
    Calculate basic statistics for a list of values
    
    Args:
        values: List of numeric values
        
    Returns:
        dict: Statistics dictionary
        {
            'min': 1.0,
            'max': 10.0,
            'avg': 5.5,
            'count': 10
        }
    """
    try:
        if not values:
            return {'min': None, 'max': None, 'avg': None, 'count': 0}
        
        return {
            'min': min(values),
            'max': max(values),
            'avg': sum(values) / len(values),
            'count': len(values)
        }
    
    except Exception as e:
        logger.error(f"Error calculating statistics: {e}")
        return {'min': None, 'max': None, 'avg': None, 'count': 0}


def validate_sample_data(data: Dict) -> Tuple[bool, str]:
    """
    Validate sample data structure
    
    Args:
        data: Sample data dictionary
        
    Returns:
        tuple: (is_valid, error_message)
    """
    try:
        # Check required fields
        if 'timestamp' not in data:
            return False, "Missing 'timestamp' field"
        
        if 'samples' not in data and 'values' not in data:
            return False, "Missing 'samples' or 'values' field"
        
        # Check data types
        if not isinstance(data.get('timestamp'), (int, float)):
            return False, "'timestamp' must be numeric"
        
        # Check aggregated data
        if data.get('is_aggregated'):
            required_agg_fields = ['sample_count', 'min_values', 'avg_values', 'max_values']
            for field in required_agg_fields:
                if field not in data:
                    return False, f"Missing '{field}' in aggregated data"
        
        return True, "Valid"
    
    except Exception as e:
        return False, f"Validation error: {e}"


def serialize_diagnostic_data(diagnostics: Dict) -> bytes:
    """
    Serialize diagnostic data for storage
    
    Args:
        diagnostics: Diagnostic data dictionary
        
    Returns:
        bytes: Serialized data
    """
    try:
        # For now, just convert to string (can be enhanced later)
        import json
        return json.dumps(diagnostics).encode('utf-8')
    except Exception as e:
        logger.error(f"Error serializing diagnostics: {e}")
        return b''


def deserialize_diagnostic_data(data: bytes) -> Optional[Dict]:
    """
    Deserialize diagnostic data from storage
    
    Args:
        data: Serialized data bytes
        
    Returns:
        dict: Diagnostic data or None if invalid
    """
    try:
        import json
        return json.loads(data.decode('utf-8'))
    except Exception as e:
        logger.error(f"Error deserializing diagnostics: {e}")
        return None


# Register name mappings (for reference)
REGISTER_NAMES = [
    'Voltage',
    'Current',
    'ActivePower',
    'ReactivePower',
    'ApparentPower',
    'PowerFactor',
    'Frequency',
    'ImportEnergy',
    'ExportEnergy',
    'TotalSystemPower'
]


# Export functions
__all__ = [
    'deserialize_aggregated_sample',
    'expand_aggregated_to_samples',
    'process_register_data',
    'format_timestamp',
    'calculate_statistics',
    'validate_sample_data',
    'serialize_diagnostic_data',
    'deserialize_diagnostic_data',
    'REGISTER_NAMES'
]
