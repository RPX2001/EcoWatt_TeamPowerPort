"""
Compression utilities for EcoWatt diagnostics
Handles decompression of various compression methods used by ESP32
"""

import struct
import base64
from typing import Dict, List, Tuple, Optional
import logging

logger = logging.getLogger(__name__)

# ==================== TEMPORAL STATE MANAGEMENT ====================
class TemporalStateManager:
    """
    Manages temporal state for delta reconstruction across packets.
    Each device maintains its own temporal buffer.
    """
    def __init__(self):
        self._device_states = {}  # device_id -> state_dict
    
    def get_base_values(self, device_id: str, register_ids: List[int]) -> Optional[List[float]]:
        """Get the last base values for a device with specific register layout"""
        if device_id not in self._device_states:
            return None
        
        state = self._device_states[device_id]
        # Check if register layout matches
        if state.get('register_ids') != register_ids:
            logger.warning(f"Register layout changed for {device_id}, cannot reconstruct deltas")
            return None
        
        return state.get('base_values')
    
    def store_base_values(self, device_id: str, register_ids: List[int], values: List[float]):
        """Store base values for a device"""
        self._device_states[device_id] = {
            'register_ids': register_ids,
            'base_values': values.copy(),
            'last_update': None  # Could add timestamp if needed
        }
        logger.debug(f"Stored base values for {device_id}: {len(values)} values")
    
    def clear_device_state(self, device_id: str):
        """Clear state for a specific device"""
        if device_id in self._device_states:
            del self._device_states[device_id]
    
    def clear_all(self):
        """Clear all device states"""
        self._device_states.clear()

# Global temporal state manager instance
_temporal_state = TemporalStateManager()


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


def decompress_temporal_delta(binary_data: bytes, device_id: str = None) -> Tuple[List[float], Dict]:
    """
    Decompress data encoded with Temporal Delta method (0xDE/0x70/0x71 markers)
    
    Args:
        binary_data: Compressed binary data
        device_id: Device identifier for stateful delta reconstruction (required for 0x71)
        
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
        # Check marker (support both old 0xDE and ESP32's 0x70/0x71)
        marker = binary_data[0]
        if marker != 0xDE and marker != 0x70 and marker != 0x71:
            logger.error(f"Invalid Temporal Delta marker: 0x{marker:02X} (expected 0xDE/0x70/0x71)")
            return [], stats
        
        # Handle ESP32 temporal compression (0x70 = base, 0x71 = delta)
        if marker == 0x70 or marker == 0x71:
            # ESP32 format: [marker][count][data...]
            # For 0x70: [reg_ids...][values...]
            # For 0x71: [variable_encoded_deltas...]
            num_samples = binary_data[1]
            
            if marker == 0x70:
                # Base sample: contains register layout + full values
                offset = 2
                # Extract register IDs (count bytes)
                reg_ids = list(binary_data[offset:offset+num_samples])
                offset += num_samples
                
                # Extract full 16-bit values
                decompressed_values = []
                for i in range(num_samples):
                    if offset + 2 > len(binary_data):
                        logger.error("Truncated base data")
                        return [], stats
                    value = struct.unpack('<H', binary_data[offset:offset+2])[0]  # Unsigned 16-bit
                    decompressed_values.append(float(value))
                    offset += 2
                
                # Store base values for future delta reconstruction
                if device_id:
                    _temporal_state.store_base_values(device_id, reg_ids, decompressed_values)
                    logger.debug(f"Stored base values for {device_id}: {reg_ids}")
                
                stats['success'] = True
                stats['original_size'] = num_samples * 2
                stats['ratio'] = len(binary_data) / (num_samples * 2) if num_samples > 0 else 0
                logger.info(f"ESP32 Temporal Base (0x70): {len(decompressed_values)} values")
                return decompressed_values, stats
            
            else:  # marker == 0x71
                # Delta sample: variable-length encoded deltas
                # Need base values to reconstruct actual values
                if not device_id:
                    logger.warning("Device ID not provided for delta reconstruction (0x71), returning deltas only")
                
                # Decode variable-length deltas
                offset = 2
                deltas = []
                
                for i in range(num_samples):
                    if offset >= len(binary_data):
                        logger.error("Truncated delta data")
                        break
                    
                    byte = binary_data[offset]
                    
                    # Check encoding type (from ESP32's variable-length encoding)
                    if byte & 0x80:  # 7-bit encoding
                        delta = byte & 0x3F
                        if byte & 0x40:
                            delta = -delta
                        offset += 1
                    elif byte == 0x00:  # 8-bit marker
                        if offset + 1 >= len(binary_data):
                            logger.error("Truncated 8-bit delta")
                            break
                        delta = struct.unpack('b', binary_data[offset+1:offset+2])[0]
                        offset += 2
                    elif byte == 0x01:  # 16-bit marker
                        if offset + 3 > len(binary_data):
                            logger.error("Truncated 16-bit delta")
                            break
                        delta = struct.unpack('<h', binary_data[offset+1:offset+3])[0]
                        offset += 3
                    else:
                        # Assume 16-bit delta without marker (fallback)
                        if offset + 2 > len(binary_data):
                            logger.error("Truncated 16-bit delta (no marker)")
                            break
                        delta = struct.unpack('<h', binary_data[offset:offset+2])[0]
                        offset += 2
                    
                    deltas.append(delta)
                
                # Try to reconstruct actual values from deltas
                if device_id:
                    base_values = _temporal_state.get_base_values(device_id, None)  # reg_ids unknown in delta packet
                    
                    if base_values and len(base_values) == len(deltas):
                        # Reconstruct: ESP32 uses linear prediction: predicted = 2*prev - prev2
                        # Then: delta = actual - predicted
                        # So: actual = delta + predicted ≈ delta + prev (simplified)
                        decompressed_values = []
                        for i, delta in enumerate(deltas):
                            reconstructed = base_values[i] + delta
                            # Clamp to uint16 range
                            reconstructed = max(0, min(65535, reconstructed))
                            decompressed_values.append(float(reconstructed))
                        
                        # Update base values with reconstructed values for next delta
                        _temporal_state.store_base_values(device_id, None, decompressed_values)
                        
                        stats['success'] = True
                        stats['original_size'] = num_samples * 2
                        stats['ratio'] = len(binary_data) / (num_samples * 2) if num_samples > 0 else 0
                        logger.info(f"ESP32 Temporal Delta (0x71): {len(decompressed_values)} values reconstructed")
                        return decompressed_values, stats
                    else:
                        logger.warning(f"Cannot reconstruct deltas: no base values (base={base_values is not None}, deltas={len(deltas)})")
                
                # Fallback: return deltas as-is (will be zeros or small values)
                decompressed_values = [float(d) for d in deltas]
                stats['success'] = True
                stats['warning'] = 'Deltas only, no reconstruction possible'
                stats['original_size'] = num_samples * 2
                stats['ratio'] = len(binary_data) / (num_samples * 2) if num_samples > 0 else 0
                logger.info(f"ESP32 Temporal Delta (0x71): {len(deltas)} deltas decoded (no reconstruction)")
                return decompressed_values, stats
        
        # Original 0xDE format handling
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
        
        # Parse header - ESP32 format: [marker][bitsPerValue][count]
        bits_per_sample = binary_data[1]  # ESP32 sends bits first
        num_samples = binary_data[2]       # Then count
        
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
    Unpack a specific number of bits from a byte buffer (MSB-first, matching ESP32)
    
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
        bit_in_byte = 7 - ((bit_offset + i) % 8)  # MSB-first (ESP32 style)
        
        if byte_idx < len(buffer):
            bit_value = (buffer[byte_idx] >> bit_in_byte) & 1
            value |= (bit_value << (num_bits - 1 - i))  # Build value MSB-first
    
    return value


def unpack_bits_from_buffer(buffer: bytes, bit_offset: int, num_bits: int) -> int:
    """
    Unpack a specific number of bits from a byte buffer (MSB-first, matching ESP32)
    
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
        bit_in_byte = 7 - ((bit_offset + i) % 8)  # MSB-first (ESP32 style)
        
        if byte_idx < len(buffer):
            bit_value = (buffer[byte_idx] >> bit_in_byte) & 1
            value |= (bit_value << (num_bits - 1 - i))  # Build value MSB-first
    
    return value


def decompress_smart_binary_data(base64_data: str, device_id: str = None) -> Tuple[Optional[List[float]], Dict]:
    """
    Auto-detect compression method and decompress
    
    Args:
        base64_data: Base64-encoded compressed data
        device_id: Device identifier for stateful decompression (optional but recommended for temporal delta)
        
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
        
        # Log the full packet for debugging
        logger.debug(f"Received {len(binary_data)} bytes, marker: 0x{marker:02X}, device: {device_id}")
        
        if marker == 0xD0:
            return decompress_dictionary_bitmask(binary_data)
        elif marker == 0xDE or marker == 0x70 or marker == 0x71:
            # 0xDE = old marker, 0x70 = ESP32 temporal base, 0x71 = ESP32 temporal delta
            # Pass device_id for stateful delta reconstruction
            return decompress_temporal_delta(binary_data, device_id=device_id)
        elif marker == 0xAD or marker == 0x50:
            return decompress_semantic_rle(binary_data)
        elif marker == 0xBF or marker == 0x01:
            # Both 0xBF and 0x01 are bitpack markers (ESP32 uses 0x01)
            return decompress_bit_packed(binary_data)
        elif marker == 0x00:
            # Raw binary format is DEPRECATED - should not be used anymore
            logger.warning("⚠ Raw binary format (0x00) detected - DEPRECATED! ESP32 should always compress.")
            logger.warning("This may indicate an old firmware or configuration issue.")
            return None, {'error': 'Raw binary format deprecated - please update ESP32 firmware'}
        else:
            logger.error(f"Unknown compression marker: 0x{marker:02X}")
            # Log more context for debugging
            logger.error(f"Full packet (hex): {binary_data.hex()}")
            logger.error(f"Full packet (bytes): {list(binary_data)}")
            return None, {'error': f'Unknown marker: 0x{marker:02X}'}
            
    except Exception as e:
        logger.error(f"Decompression failed: {e}")
        return None, {'error': str(e)}
