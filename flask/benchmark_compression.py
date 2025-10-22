"""
Compression Benchmark Tool for EcoWatt Flask Server

Python implementation of ESP32 compression algorithms for cross-validation.
Tests all compression methods with the same datasets as ESP32 compression_test.cpp.

Author: EcoWatt Team
Date: October 22, 2025
"""

import base64
import struct
import time
from typing import List, Tuple, Dict, Any


class CompressionMetrics:
    """Track compression performance metrics"""
    
    def __init__(self):
        self.original_size = 0
        self.compressed_size = 0
        self.compression_time = 0.0
        self.decompression_time = 0.0
        self.lossless = False
        
    def calculate(self, original_data: List[int], compressed_data: bytes, 
                 comp_time: float, decomp_time: float, decompressed_data: List[int]):
        """Calculate all metrics"""
        self.original_size = len(original_data) * 2  # uint16_t = 2 bytes
        self.compressed_size = len(base64.b64encode(compressed_data))  # Base64 encoded size
        self.compression_time = comp_time * 1000  # Convert to ms
        self.decompression_time = decomp_time * 1000  # Convert to ms
        self.lossless = (original_data == decompressed_data)
        
    @property
    def academic_ratio(self) -> float:
        """Compressed / Original (< 1 is good)"""
        return self.compressed_size / self.original_size if self.original_size > 0 else 0
    
    @property
    def traditional_ratio(self) -> float:
        """Original / Compressed (> 1 is good)"""
        return self.original_size / self.compressed_size if self.compressed_size > 0 else 0
    
    @property
    def savings_percent(self) -> float:
        """Percentage saved"""
        return (1 - self.academic_ratio) * 100
    
    def print(self, method_name: str, dataset_name: str):
        """Print formatted metrics"""
        print(f"\n{'='*60}")
        print(f"Dataset: {dataset_name}")
        print(f"Method: {method_name}")
        print(f"{'-'*60}")
        print(f"Original Size:     {self.original_size:6d} bytes")
        print(f"Compressed Size:   {self.compressed_size:6d} bytes")
        print(f"Academic Ratio:    {self.academic_ratio:6.3f} ({self.academic_ratio*100:.1f}%)")
        print(f"Traditional Ratio: {self.traditional_ratio:6.1f}:1")
        print(f"Compression Savings: {self.savings_percent:.1f}%")
        print(f"{'-'*60}")
        print(f"Compression Time:   {self.compression_time:6.2f} ms")
        print(f"Decompression Time: {self.decompression_time:6.2f} ms")
        print(f"Lossless Test:      {'✅ PASSED' if self.lossless else '❌ FAILED'}")
        print(f"{'='*60}\n")


# ============================================================================
# DICTIONARY + BITMASK COMPRESSION
# ============================================================================

DICTIONARY_PATTERNS = [
    [2400, 170, 70, 4000, 65, 550, 0, 0, 0, 0],      # Pattern 0
    [2380, 100, 50, 2000, 25, 520, 0, 0, 0, 0],      # Pattern 1
    [2420, 200, 100, 5000, 80, 580, 0, 0, 0, 0],     # Pattern 2
    [2350, 150, 60, 3000, 50, 500, 0, 0, 0, 0],      # Pattern 3
]


def compress_dictionary_bitmask(data: List[int]) -> bytes:
    """
    Dictionary + Bitmask compression
    Format: 0xD0 | pattern_id | count | bitmask_low | bitmask_high | [deltas...]
    """
    if len(data) < 10:
        return bytes()
    
    # Find best matching pattern
    best_pattern_id = 0
    best_score = float('inf')
    
    for pattern_id, pattern in enumerate(DICTIONARY_PATTERNS):
        score = sum(abs(data[i] - pattern[i]) for i in range(min(10, len(data))))
        if score < best_score:
            best_score = score
            best_pattern_id = pattern_id
    
    pattern = DICTIONARY_PATTERNS[best_pattern_id]
    
    # Calculate bitmask and deltas
    bitmask = 0
    deltas = []
    
    for i in range(min(10, len(data))):
        delta = data[i] - pattern[i]
        if delta != 0:
            bitmask |= (1 << i)
            # Store as signed 16-bit
            delta_bytes = struct.pack('<h', delta)
            deltas.extend(delta_bytes)
    
    # Build compressed frame
    result = bytearray()
    result.append(0xD0)  # Method ID
    result.append(best_pattern_id)
    result.append(len(data) // 10)  # Count of samples
    result.append(bitmask & 0xFF)
    result.append((bitmask >> 8) & 0xFF)
    result.extend(deltas)
    
    return bytes(result)


def decompress_dictionary_bitmask(data: bytes) -> List[int]:
    """Decompress dictionary + bitmask data"""
    if len(data) < 5 or data[0] != 0xD0:
        return []
    
    pattern_id = data[1]
    count = data[2]
    bitmask = data[3] | (data[4] << 8)
    
    if pattern_id >= len(DICTIONARY_PATTERNS):
        return []
    
    pattern = DICTIONARY_PATTERNS[pattern_id]
    result = []
    delta_idx = 5
    
    for sample in range(count):
        for reg in range(10):
            if bitmask & (1 << reg):
                # Read delta
                if delta_idx + 1 < len(data):
                    delta = struct.unpack('<h', data[delta_idx:delta_idx+2])[0]
                    delta_idx += 2
                else:
                    delta = 0
                result.append(pattern[reg] + delta)
            else:
                result.append(pattern[reg])
    
    return result


# ============================================================================
# TEMPORAL DELTA COMPRESSION
# ============================================================================

def compress_temporal_delta(data: List[int]) -> bytes:
    """
    Temporal delta compression
    Format: 0xDE | count | base_values[10] | deltas...
    """
    if len(data) < 10:
        return bytes()
    
    result = bytearray()
    result.append(0xDE)  # Method ID
    result.append(len(data) // 10)  # Count
    
    # Store first sample as base
    for i in range(10):
        result.extend(struct.pack('<H', data[i]))
    
    # Store deltas for subsequent samples
    if len(data) > 10:
        for i in range(10, len(data)):
            prev_val = data[i - 10]
            delta = data[i] - prev_val
            result.extend(struct.pack('<h', delta))
    
    return bytes(result)


def decompress_temporal_delta(data: bytes) -> List[int]:
    """Decompress temporal delta data"""
    if len(data) < 2 or data[0] != 0xDE:
        return []
    
    count = data[1]
    result = []
    idx = 2
    
    # Read base values
    for i in range(10):
        if idx + 1 < len(data):
            val = struct.unpack('<H', data[idx:idx+2])[0]
            result.append(val)
            idx += 2
    
    # Read deltas and reconstruct
    for sample in range(1, count):
        for reg in range(10):
            if idx + 1 < len(data):
                delta = struct.unpack('<h', data[idx:idx+2])[0]
                idx += 2
                result.append(result[-10] + delta)
            else:
                result.append(result[-10])
    
    return result


# ============================================================================
# SEMANTIC RLE COMPRESSION
# ============================================================================

def compress_semantic_rle(data: List[int]) -> bytes:
    """
    Semantic RLE compression
    Format: 0xRL | count | [reg_id | run_length | value]...
    """
    if len(data) < 10:
        return bytes()
    
    result = bytearray()
    result.append(0xAD)  # Method ID (0xRL would be 0x52 0x4C, using 0xAD)
    result.append(len(data) // 10)
    
    # For each register position
    for reg in range(10):
        runs = []
        current_val = None
        run_length = 0
        
        # Collect values for this register across samples
        for sample in range(len(data) // 10):
            idx = sample * 10 + reg
            if idx < len(data):
                val = data[idx]
                if val == current_val:
                    run_length += 1
                else:
                    if current_val is not None:
                        runs.append((run_length, current_val))
                    current_val = val
                    run_length = 1
        
        if current_val is not None:
            runs.append((run_length, current_val))
        
        # Encode runs
        result.append(reg)
        result.append(len(runs))
        for run_len, val in runs:
            result.append(run_len)
            result.extend(struct.pack('<H', val))
    
    return bytes(result)


def decompress_semantic_rle(data: bytes) -> List[int]:
    """Decompress semantic RLE data"""
    if len(data) < 2 or data[0] != 0xAD:
        return []
    
    count = data[1]
    registers = [[] for _ in range(10)]
    idx = 2
    
    # Read runs for each register
    for _ in range(10):
        if idx >= len(data):
            break
        reg_id = data[idx]
        idx += 1
        if idx >= len(data):
            break
        num_runs = data[idx]
        idx += 1
        
        for _ in range(num_runs):
            if idx + 2 >= len(data):
                break
            run_len = data[idx]
            idx += 1
            val = struct.unpack('<H', data[idx:idx+2])[0]
            idx += 2
            
            if reg_id < 10:
                registers[reg_id].extend([val] * run_len)
    
    # Interleave registers
    result = []
    for sample in range(count):
        for reg in range(10):
            if sample < len(registers[reg]):
                result.append(registers[reg][sample])
            else:
                result.append(0)
    
    return result


# ============================================================================
# BINARY AUTO-SELECT COMPRESSION
# ============================================================================

def compress_binary_auto(data: List[int]) -> bytes:
    """
    Binary auto-select - tries multiple methods and picks best
    """
    methods = [
        (compress_dictionary_bitmask, "Dictionary"),
        (compress_temporal_delta, "Delta"),
        (compress_semantic_rle, "RLE"),
    ]
    
    best_compressed = None
    best_size = float('inf')
    
    for compress_func, name in methods:
        try:
            compressed = compress_func(data)
            if compressed and len(compressed) < best_size:
                best_size = len(compressed)
                best_compressed = compressed
        except:
            pass
    
    return best_compressed if best_compressed else bytes()


# ============================================================================
# TEST DATASETS
# ============================================================================

# Dataset 1: Constant values (worst for delta, best for RLE)
CONSTANT_DATA = [2400, 170, 70, 4000, 65, 550, 0, 0, 0, 0] * 7

# Dataset 2: Linear ramp (best for delta)
LINEAR_DATA = []
for i in range(7):
    LINEAR_DATA.extend([2400+i*10, 170+i*5, 70+i*2, 4000+i*20, 65+i, 550+i*3, 0, 0, 0, 0])

# Dataset 3: Realistic inverter data
REALISTIC_DATA = [
    2405, 172, 68, 4010, 67, 555, 0, 0, 0, 0,
    2403, 170, 69, 4005, 66, 552, 0, 0, 0, 0,
    2407, 175, 72, 4015, 68, 558, 0, 0, 0, 0,
    2402, 168, 67, 4000, 65, 550, 0, 0, 0, 0,
    2410, 180, 75, 4020, 70, 562, 0, 0, 0, 0,
    2400, 165, 65, 3995, 64, 548, 0, 0, 0, 0,
    2408, 177, 73, 4012, 69, 560, 0, 0, 0, 0,
]

# Dataset 4: Random noise (worst case)
import random
random.seed(42)
RANDOM_DATA = []
for _ in range(7):
    RANDOM_DATA.extend([
        random.randint(2000, 2800),
        random.randint(50, 300),
        random.randint(30, 150),
        random.randint(3000, 5000),
        random.randint(20, 100),
        random.randint(400, 700),
        0, 0, 0, 0
    ])


# ============================================================================
# TEST RUNNER
# ============================================================================

def test_compression_method(method_name: str, compress_func, decompress_func,
                           data: List[int], dataset_name: str) -> CompressionMetrics:
    """Test a single compression method"""
    metrics = CompressionMetrics()
    
    # Compress
    start = time.time()
    compressed = compress_func(data)
    comp_time = time.time() - start
    
    # Decompress
    start = time.time()
    decompressed = decompress_func(compressed)
    decomp_time = time.time() - start
    
    # Calculate metrics
    metrics.calculate(data, compressed, comp_time, decomp_time, decompressed)
    
    # Print results
    metrics.print(method_name, dataset_name)
    
    return metrics


def run_benchmark():
    """Run full compression benchmark"""
    print("\n" + "="*60)
    print("COMPRESSION BENCHMARK - Python Validation")
    print("="*60)
    
    datasets = [
        ("Constant Values", CONSTANT_DATA),
        ("Linear Ramp", LINEAR_DATA),
        ("Realistic Inverter Data", REALISTIC_DATA),
        ("Random Noise", RANDOM_DATA),
    ]
    
    methods = [
        ("Dictionary + Bitmask", compress_dictionary_bitmask, decompress_dictionary_bitmask),
        ("Temporal Delta", compress_temporal_delta, decompress_temporal_delta),
        ("Semantic RLE", compress_semantic_rle, decompress_semantic_rle),
        ("Binary Auto-Select", compress_binary_auto, None),
    ]
    
    results = {}
    
    for dataset_name, data in datasets:
        print(f"\n{'#'*60}")
        print(f"# Testing Dataset: {dataset_name}")
        print(f"# Samples: {len(data)//10}, Registers: 10")
        print(f"{'#'*60}")
        
        dataset_results = []
        
        for method_name, compress_func, decompress_func in methods:
            if decompress_func is None:  # Binary auto-select
                # Try to decompress with all methods
                compressed = compress_func(data)
                decompressed = []
                if compressed:
                    if compressed[0] == 0xD0:
                        decompressed = decompress_dictionary_bitmask(compressed)
                    elif compressed[0] == 0xDE:
                        decompressed = decompress_temporal_delta(compressed)
                    elif compressed[0] == 0xAD:
                        decompressed = decompress_semantic_rle(compressed)
                
                metrics = CompressionMetrics()
                start = time.time()
                compressed = compress_func(data)
                comp_time = time.time() - start
                metrics.calculate(data, compressed, comp_time, 0, decompressed)
                metrics.print(method_name, dataset_name)
                dataset_results.append((method_name, metrics))
            else:
                metrics = test_compression_method(method_name, compress_func, 
                                                 decompress_func, data, dataset_name)
                dataset_results.append((method_name, metrics))
        
        # Find best method for this dataset
        best_method = min(dataset_results, key=lambda x: x[1].compressed_size)
        print(f"\n🏆 Best method for {dataset_name}: {best_method[0]}")
        print(f"   Compression: {best_method[1].savings_percent:.1f}% savings")
        
        results[dataset_name] = dataset_results
    
    # Summary
    print(f"\n{'='*60}")
    print("SUMMARY")
    print(f"{'='*60}")
    for dataset_name, dataset_results in results.items():
        print(f"\n{dataset_name}:")
        for method_name, metrics in dataset_results:
            status = "✅" if metrics.lossless else "❌"
            print(f"  {status} {method_name:25s}: {metrics.savings_percent:5.1f}% savings")


if __name__ == "__main__":
    run_benchmark()
