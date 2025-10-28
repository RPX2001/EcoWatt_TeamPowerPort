"""
Compression utilities for EcoWatt diagnostics
Handles decompression of various compression methods used by ESP32
"""

import struct
import base64
from typing import Dict, List, Tuple, Optional
import logging

logger = logging.getLogger(__name__)


def decompress_dictionary_bitmask(binary_data: bytes) -> Tuple[List[float], Dict]:
    """
    Decompress data encoded with Dictionary + Bitmask method (0xD0 marker)
    
    Args:
        binary_data: Compressed binary data
        
    Returns:
        Tuple of (decompressed_values, stats_dict)
    """
    stats = {
        'method': 'Dictionary + Bitmask',
        'compressed_size': len(binary_data),
        'success': False
    }
    
    if len(binary_data) < 5:
        logger.error("Dictionary + Bitmask data too short")
        return [], stats
    
    try:
        # Check marker
        marker = binary_data[0]
        if marker != 0xD0:
            logger.error(f"Invalid Dictionary + Bitmask marker: 0x{marker:02X}")
            return [], stats
        
        # Parse header
        num_samples = binary_data[1]
        num_patterns = binary_data[2]
        
        logger.info(f"Dictionary + Bitmask: {num_samples} samples, {num_patterns} patterns")
        
        # Extract dictionary patterns (float values)
        dictionary = []
        offset = 3
        for i in range(num_patterns):
            if offset + 4 > len(binary_data):
                logger.error("Truncated dictionary data")
                return [], stats
            value = struct.unpack('<f', binary_data[offset:offset+4])[0]
            dictionary.append(value)
            offset += 4
        
        logger.debug(f"Dictionary: {dictionary}")
        
        # Calculate bitmask size
        bits_per_index = num_patterns.bit_length()
        total_bits = num_samples * bits_per_index
        bitmask_bytes = (total_bits + 7) // 8
        
        if offset + bitmask_bytes > len(binary_data):
            logger.error(f"Truncated bitmask: need {bitmask_bytes} bytes, have {len(binary_data) - offset}")
            return [], stats
        
        bitmask = binary_data[offset:offset+bitmask_bytes]
        
        # Decode bitmask to reconstruct values
        decompressed_values = []
        bit_pos = 0
        
        for sample_idx in range(num_samples):
            # Extract bits_per_index bits from bitmask
            index = 0
            for bit in range(bits_per_index):
                byte_idx = bit_pos // 8
                bit_in_byte = bit_pos % 8
                
                if byte_idx < len(bitmask):
                    bit_value = (bitmask[byte_idx] >> bit_in_byte) & 1
                    index |= (bit_value << bit)
                
                bit_pos += 1
            
            # Look up value in dictionary
            if index < len(dictionary):
                decompressed_values.append(dictionary[index])
            else:
                logger.warning(f"Invalid dictionary index: {index}")
                decompressed_values.append(0.0)
        
        stats['success'] = True
        stats['original_size'] = num_samples * 4
        stats['ratio'] = len(binary_data) / (num_samples * 4) if num_samples > 0 else 0
        
        logger.info(f"Dictionary + Bitmask decompression: {len(decompressed_values)} values")
        return decompressed_values, stats
        
    except Exception as e:
        logger.error(f"Dictionary + Bitmask decompression failed: {e}")
        return [], stats


def decompress_temporal_delta(binary_data: bytes) -> Tuple[List[float], Dict]:
    """
    Decompress data encoded with Temporal Delta method (0xDE marker)
    
    Args:
        binary_data: Compressed binary data
        
    Returns:
        Tuple of (decompressed_values, stats_dict)
    """
    stats = {
        'method': 'Temporal Delta',
        'compressed_size': len(binary_data),
        'success': False
    }
    
    if len(binary_data) < 6:
        logger.error("Temporal Delta data too short")
        return [], stats
    
    try:
        # Check marker
        marker = binary_data[0]
        if marker != 0xDE:
            logger.error(f"Invalid Temporal Delta marker: 0x{marker:02X}")
            return [], stats
        
        # Parse header
        num_samples = binary_data[1]
        base_value = struct.unpack('<f', binary_data[2:6])[0]
        
        logger.info(f"Temporal Delta: {num_samples} samples, base={base_value}")
        
        # Extract deltas (int16 values)
        offset = 6
        decompressed_values = [base_value]
        
        for i in range(num_samples - 1):
            if offset + 2 > len(binary_data):
                logger.error("Truncated delta data")
                return [], stats
            
            delta = struct.unpack('<h', binary_data[offset:offset+2])[0]
            next_value = decompressed_values[-1] + (delta / 100.0)
            decompressed_values.append(next_value)
            offset += 2
        
        stats['success'] = True
        stats['original_size'] = num_samples * 4
        stats['ratio'] = len(binary_data) / (num_samples * 4) if num_samples > 0 else 0
        
        logger.info(f"Temporal Delta decompression: {len(decompressed_values)} values")
        return decompressed_values, stats
        
    except Exception as e:
        logger.error(f"Temporal Delta decompression failed: {e}")
        return [], stats


def decompress_semantic_rle(binary_data: bytes) -> Tuple[List[float], Dict]:
    """
    Decompress data encoded with Semantic RLE method (0xAD marker)
    
    Args:
        binary_data: Compressed binary data
        
    Returns:
        Tuple of (decompressed_values, stats_dict)
    """
    stats = {
        'method': 'Semantic RLE',
        'compressed_size': len(binary_data),
        'success': False
    }
    
    if len(binary_data) < 2:
        logger.error("Semantic RLE data too short")
        return [], stats
    
    try:
        # Check marker
        marker = binary_data[0]
        if marker != 0xAD:
            logger.error(f"Invalid Semantic RLE marker: 0x{marker:02X}")
            return [], stats
        
        # Parse header
        total_samples = binary_data[1]
        
        logger.info(f"Semantic RLE: {total_samples} total samples")
        
        # Decode run-length encoding
        decompressed_values = []
        offset = 2
        
        while offset < len(binary_data) and len(decompressed_values) < total_samples:
            if offset + 5 > len(binary_data):
                logger.error("Truncated RLE data")
                break
            
            # Read value and count
            value = struct.unpack('<f', binary_data[offset:offset+4])[0]
            count = binary_data[offset+4]
            offset += 5
            
            # Expand run
            for _ in range(count):
                if len(decompressed_values) < total_samples:
                    decompressed_values.append(value)
        
        stats['success'] = True
        stats['original_size'] = total_samples * 4
        stats['ratio'] = len(binary_data) / (total_samples * 4) if total_samples > 0 else 0
        
        logger.info(f"Semantic RLE decompression: {len(decompressed_values)} values")
        return decompressed_values, stats
        
    except Exception as e:
        logger.error(f"Semantic RLE decompression failed: {e}")
        return [], stats


def decompress_bit_packed(binary_data: bytes) -> Tuple[List[float], Dict]:
    """
    Decompress data encoded with Bit Packing method (0xBF marker)
    
    Args:
        binary_data: Compressed binary data
        
    Returns:
        Tuple of (decompressed_values, stats_dict)
    """
    stats = {
        'method': 'Bit Packing',
        'compressed_size': len(binary_data),
        'success': False
    }
    
    if len(binary_data) < 3:
        logger.error("Bit Packed data too short")
        return [], stats
    
    try:
        # Check marker (accept both 0xBF and 0x01)
        marker = binary_data[0]
        if marker != 0xBF and marker != 0x01:
            logger.error(f"Invalid Bit Packing marker: 0x{marker:02X}")
            return [], stats
        
        # Parse header
        num_samples = binary_data[1]
        bits_per_sample = binary_data[2]
        
        logger.info(f"Bit Packing: {num_samples} samples, {bits_per_sample} bits each")
        
        # Unpack bits
        decompressed_values = []
        bit_offset = 0
        offset = 3
        
        for i in range(num_samples):
            value = unpack_bits_from_buffer(binary_data, offset * 8 + bit_offset, bits_per_sample)
            decompressed_values.append(float(value))
            bit_offset += bits_per_sample
            
            if bit_offset >= 8:
                offset += bit_offset // 8
                bit_offset = bit_offset % 8
        
        stats['success'] = True
        stats['original_size'] = num_samples * 4
        stats['ratio'] = len(binary_data) / (num_samples * 4) if num_samples > 0 else 0
        
        logger.info(f"Bit Packing decompression: {len(decompressed_values)} values")
        return decompressed_values, stats
        
    except Exception as e:
        logger.error(f"Bit Packing decompression failed: {e}")
        return [], stats


def unpack_bits_from_buffer(buffer: bytes, bit_offset: int, num_bits: int) -> int:
    """
    Unpack a specific number of bits from a byte buffer
    
    Args:
        buffer: Byte buffer
        bit_offset: Starting bit position
        num_bits: Number of bits to extract
        
    Returns:
        Extracted integer value
    """
    value = 0
    for i in range(num_bits):
        byte_idx = (bit_offset + i) // 8
        bit_in_byte = (bit_offset + i) % 8
        
        if byte_idx < len(buffer):
            bit_value = (buffer[byte_idx] >> bit_in_byte) & 1
            value |= (bit_value << i)
    
    return value


def decompress_smart_binary_data(base64_data: str) -> Tuple[Optional[List[float]], Dict]:
    """
    Auto-detect compression method and decompress
    
    Args:
        base64_data: Base64-encoded compressed data
        
    Returns:
        Tuple of (decompressed_values or None, stats_dict)
    """
    try:
        # Decode base64
        binary_data = base64.b64decode(base64_data)
        
        if len(binary_data) < 1:
            logger.error("Empty binary data")
            return None, {'error': 'Empty data'}
        
        # Check compression marker
        marker = binary_data[0]
        
        if marker == 0xD0:
            return decompress_dictionary_bitmask(binary_data)
        elif marker == 0xDE:
            return decompress_temporal_delta(binary_data)
        elif marker == 0xAD:
            return decompress_semantic_rle(binary_data)
        elif marker == 0xBF or marker == 0x01:
            # Both 0xBF and 0x01 are bitpack markers (ESP32 uses 0x01)
            return decompress_bit_packed(binary_data)
        else:
            logger.error(f"Unknown compression marker: 0x{marker:02X}")
            return None, {'error': f'Unknown marker: 0x{marker:02X}'}
            
    except Exception as e:
        logger.error(f"Decompression failed: {e}")
        return None, {'error': str(e)}
