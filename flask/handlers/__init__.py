"""
Handlers package for EcoWatt Flask server
Provides business logic layer between routes and utilities
"""

# Import all handler functions
from .compression_handler import (
    handle_compressed_data,
    validate_compression_crc,
    get_compression_statistics,
    reset_compression_statistics,
    handle_aggregated_data
)

from .diagnostics_handler import (
    store_diagnostics,
    get_diagnostics_by_device,
    get_all_diagnostics,
    clear_diagnostics,
    get_diagnostics_summary,
    load_persisted_diagnostics
)

from .security_handler import (
    validate_secured_payload,
    get_security_stats,
    reset_security_stats,
    clear_nonces,
    get_device_security_info,
    cleanup_expired_nonces
)

from .ota_handler import (
    check_for_update,
    initiate_ota_session,
    get_firmware_chunk_for_device,
    complete_ota_session,
    get_ota_status,
    get_ota_stats,
    cancel_ota_session,
    enable_ota_fault_injection,
    disable_ota_fault_injection,
    get_ota_fault_status,
    VALID_OTA_FAULT_TYPES
)

from .command_handler import (
    queue_command,
    poll_commands,
    submit_command_result,
    get_command_status,
    get_command_history,
    get_command_stats,
    reset_command_stats,
    cancel_pending_commands
)

from .fault_handler import (
    enable_fault_injection,
    disable_fault_injection,
    get_fault_status,
    get_available_faults,
    get_fault_statistics,
    reset_fault_statistics
)

# Export all handler functions
__all__ = [
    # Compression handlers
    'handle_compressed_data',
    'validate_compression_crc',
    'get_compression_statistics',
    'reset_compression_statistics',
    'handle_aggregated_data',
    
    # Diagnostics handlers
    'store_diagnostics',
    'get_diagnostics_by_device',
    'get_all_diagnostics',
    'clear_diagnostics',
    'get_diagnostics_summary',
    'load_persisted_diagnostics',
    
    # Security handlers
    'validate_secured_payload',
    'get_security_stats',
    'reset_security_stats',
    'clear_nonces',
    'get_device_security_info',
    'cleanup_expired_nonces',
    
    # OTA handlers
    'check_for_update',
    'initiate_ota_session',
    'get_firmware_chunk_for_device',
    'complete_ota_session',
    'get_ota_status',
    'get_ota_stats',
    'cancel_ota_session',
    
    # Command handlers
    'queue_command',
    'poll_commands',
    'submit_command_result',
    'get_command_status',
    'get_command_history',
    'get_command_stats',
    'reset_command_stats',
    'cancel_pending_commands',
    
    # Fault handlers
    'enable_fault_injection',
    'disable_fault_injection',
    'get_fault_status',
    'get_available_faults',
    'get_fault_statistics',
    'reset_fault_statistics'
]


from .compression_handler import (
    handle_compressed_data,
    validate_compression_crc,
    get_compression_statistics
)

from .diagnostics_handler import (
    store_diagnostics,
    get_diagnostics_by_device,
    get_all_diagnostics,
    clear_diagnostics
)

__all__ = [
    # Compression
    'handle_compressed_data',
    'validate_compression_crc',
    'get_compression_statistics',
    # Diagnostics
    'store_diagnostics',
    'get_diagnostics_by_device',
    'get_all_diagnostics',
    'clear_diagnostics'
]
