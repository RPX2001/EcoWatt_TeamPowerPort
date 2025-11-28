"""
Compression handler for EcoWatt diagnostics
Orchestrates decompression and validation
"""

import logging
from typing import Dict, List, Optional, Tuple
from utils.compression_utils import decompress_smart_binary_data
from utils.data_utils import deserialize_aggregated_sample, expand_aggregated_to_samples
from utils.logger_utils import log_success
import struct

logger = logging.getLogger(__name__)

# Global compression statistics
compression_stats = {
    'total_decompressions': 0,
    'successful_decompressions': 0,
    'failed_decompressions': 0,
    'total_bytes_decompressed': 0,
    'total_bytes_original': 0,
    'methods_used': {
        'Dictionary + Bitmask': 0,
        'Temporal Delta': 0,
        'Semantic RLE': 0,
        'Bit Packing': 0,
        'Unknown': 0
    },
    'average_ratio': 0.0
}


def handle_compressed_data(base64_data: str, device_id: str = None, validate_crc: bool = True) -> Tuple[Optional[List[float]], Dict]:
    """
    Main entry point for handling compressed diagnostic data
    
    Args:
        base64_data: Base64-encoded compressed data
        device_id: Device identifier for stateful decompression (recommended for temporal delta)
        validate_crc: Whether to validate CRC32 checksum
        
    Returns:
        tuple: (decompressed_values, stats_dict)
        - decompressed_values: List of float values or None if failed
        - stats_dict: Statistics about decompression
    """
    try:
        logger.debug(f"Processing compressed data: {len(base64_data)} bytes (base64), device: {device_id}")
        
        # Update statistics
        compression_stats['total_decompressions'] += 1
        
        # Decompress data with device_id for stateful reconstruction
        decompressed_values, stats = decompress_smart_binary_data(base64_data, device_id=device_id)
        
        if decompressed_values is None:
            logger.error("✗ Decompression failed")
            compression_stats['failed_decompressions'] += 1
            return None, {'error': 'Decompression failed', 'stats': stats}
        
        # Update success statistics
        compression_stats['successful_decompressions'] += 1
        compression_stats['total_bytes_decompressed'] += stats.get('compressed_size', 0)
        compression_stats['total_bytes_original'] += stats.get('original_size', 0)
        
        # Update method usage
        method = stats.get('method', 'Unknown')
        if method in compression_stats['methods_used']:
            compression_stats['methods_used'][method] += 1
        else:
            compression_stats['methods_used']['Unknown'] += 1
        
        # Calculate running average ratio
        if compression_stats['total_bytes_original'] > 0:
            compression_stats['average_ratio'] = (
                compression_stats['total_bytes_decompressed'] / 
                compression_stats['total_bytes_original']
            )
        
        log_success(logger, "Decompressed %d values | Method: %s | Ratio: %.3f", 
                   len(decompressed_values), method, stats.get('ratio', 0))
        
        return decompressed_values, {
            'success': True,
            'method': method,
            'compressed_size': stats.get('compressed_size', 0),
            'original_size': stats.get('original_size', 0),
            'ratio': stats.get('ratio', 0),
            'value_count': len(decompressed_values)
        }
        
    except Exception as e:
        logger.error(f"✗ Error handling compressed data: {e}")
        compression_stats['failed_decompressions'] += 1
        return None, {'error': str(e)}


def validate_compression_crc(data: bytes, expected_crc: int) -> bool:
    """
    Validate CRC32 checksum of compressed data
    
    Args:
        data: Binary data to validate
        expected_crc: Expected CRC32 checksum
        
    Returns:
        bool: True if CRC matches, False otherwise
    """
    try:
        import zlib
        actual_crc = zlib.crc32(data) & 0xFFFFFFFF
        
        if actual_crc != expected_crc:
            logger.warning(f"CRC mismatch: expected 0x{expected_crc:08X}, "
                         f"got 0x{actual_crc:08X}")
            return False
        
        logger.debug(f"CRC validation passed: 0x{actual_crc:08X}")
        return True
        
    except Exception as e:
        logger.error(f"CRC validation error: {e}")
        return False


def get_compression_statistics() -> Dict:
    """
    Get current compression statistics
    
    Returns:
        dict: Compression statistics
    """
    stats = compression_stats.copy()
    
    # Calculate additional metrics
    if stats['total_decompressions'] > 0:
        stats['success_rate'] = (
            stats['successful_decompressions'] / stats['total_decompressions']
        ) * 100
    else:
        stats['success_rate'] = 0.0
    
    # Calculate savings
    if stats['total_bytes_original'] > 0:
        bytes_saved = stats['total_bytes_original'] - stats['total_bytes_decompressed']
        stats['bytes_saved'] = bytes_saved
        stats['savings_percent'] = (bytes_saved / stats['total_bytes_original']) * 100
    else:
        stats['bytes_saved'] = 0
        stats['savings_percent'] = 0.0
    
    return stats


def reset_compression_statistics():
    """
    Reset compression statistics to zero
    """
    global compression_stats
    
    compression_stats = {
        'total_decompressions': 0,
        'successful_decompressions': 0,
        'failed_decompressions': 0,
        'total_bytes_decompressed': 0,
        'total_bytes_original': 0,
        'methods_used': {
            'Dictionary + Bitmask': 0,
            'Temporal Delta': 0,
            'Semantic RLE': 0,
            'Bit Packing': 0,
            'Unknown': 0
        },
        'average_ratio': 0.0
    }
    
    logger.info("Compression statistics reset")


def handle_aggregated_data(binary_data: bytes) -> Optional[Dict]:
    """
    Handle aggregated sample data
    
    Args:
        binary_data: Binary aggregated data
        
    Returns:
        dict: Expanded sample data or None if failed
    """
    try:
        # Deserialize aggregated sample
        aggregated = deserialize_aggregated_sample(binary_data)
        
        if aggregated is None:
            logger.warning("Failed to deserialize aggregated data")
            return None
        
        # Expand to samples
        expanded = expand_aggregated_to_samples(aggregated)
        
        if expanded is None:
            logger.warning("Failed to expand aggregated data")
            return None
        
        logger.info(f"Aggregated data processed: {aggregated['sample_count']} samples")
        return expanded
        
    except Exception as e:
        logger.error(f"Error handling aggregated data: {e}")
        return None


# Export functions
__all__ = [
    'handle_compressed_data',
    'validate_compression_crc',
    'get_compression_statistics',
    'reset_compression_statistics',
    'handle_aggregated_data'
]
