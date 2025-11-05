"""
Utility modules for EcoWatt Flask server
"""

from .compression_utils import (
    decompress_dictionary_bitmask,
    decompress_temporal_delta,
    decompress_semantic_rle,
    decompress_bit_packed,
    decompress_smart_binary_data
)

from .logger_utils import (
    init_logging,
    get_logger,
    set_log_level,
    log_security_event,
    create_audit_logger
)

from .data_utils import (
    deserialize_aggregated_sample,
    expand_aggregated_to_samples,
    process_register_data,
    format_timestamp,
    calculate_statistics,
    validate_sample_data,
    REGISTER_NAMES
)

__all__ = [
    # Compression
    'decompress_dictionary_bitmask',
    'decompress_temporal_delta',
    'decompress_semantic_rle',
    'decompress_bit_packed',
    'decompress_smart_binary_data',
    # Logging
    'init_logging',
    'get_logger',
    'set_log_level',
    'log_security_event',
    'create_audit_logger',
    # Data
    'deserialize_aggregated_sample',
    'expand_aggregated_to_samples',
    'process_register_data',
    'format_timestamp',
    'calculate_statistics',
    'validate_sample_data',
    'REGISTER_NAMES'
]
