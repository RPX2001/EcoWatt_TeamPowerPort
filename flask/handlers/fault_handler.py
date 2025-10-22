"""
Fault injection handler for EcoWatt Flask server
Manages fault injection for testing and diagnostics
"""

import logging
import threading
from datetime import datetime
from typing import Dict, Optional, List, Tuple

# Import existing fault injector (if available)
try:
    from fault_injector import (
        enable_fault as fi_enable_fault,
        disable_fault as fi_disable_fault,
        get_fault_status as fi_get_fault_status,
        get_available_faults as fi_get_available_faults
    )
    FAULT_INJECTOR_AVAILABLE = True
except ImportError:
    FAULT_INJECTOR_AVAILABLE = False
    logging.warning("fault_injector module not available, using fallback implementation")

logger = logging.getLogger(__name__)

# Fault injection state
fault_state = {
    'enabled': False,
    'active_faults': [],
    'fault_history': []
}

fault_lock = threading.Lock()

# Fault statistics
fault_stats = {
    'total_faults_enabled': 0,
    'total_faults_triggered': 0,
    'active_fault_count': 0,
    'last_reset': datetime.now().isoformat()
}


def enable_fault_injection(
    fault_type: str,
    parameters: Optional[Dict] = None
) -> Tuple[bool, Optional[str]]:
    """
    Enable a specific fault injection
    
    Args:
        fault_type: Type of fault to inject
        parameters: Optional fault parameters
        
    Returns:
        tuple: (success, error_message)
    """
    try:
        if FAULT_INJECTOR_AVAILABLE:
            # Use existing fault injector
            result = fi_enable_fault(fault_type, parameters or {})
            
            if result.get('success'):
                with fault_lock:
                    fault_state['enabled'] = True
                    fault_state['active_faults'].append({
                        'type': fault_type,
                        'parameters': parameters,
                        'enabled_at': datetime.now().isoformat()
                    })
                    
                    fault_stats['total_faults_enabled'] += 1
                    fault_stats['active_fault_count'] += 1
                
                logger.info(f"Fault injection enabled: {fault_type}")
                return True, None
            else:
                error_msg = result.get('error', 'Unknown error')
                logger.error(f"Failed to enable fault injection: {error_msg}")
                return False, error_msg
        else:
            # Fallback implementation
            with fault_lock:
                fault_state['enabled'] = True
                fault_state['active_faults'].append({
                    'type': fault_type,
                    'parameters': parameters,
                    'enabled_at': datetime.now().isoformat()
                })
                
                fault_stats['total_faults_enabled'] += 1
                fault_stats['active_fault_count'] += 1
            
            logger.info(f"Fault injection enabled (fallback): {fault_type}")
            return True, None
        
    except Exception as e:
        logger.error(f"Error enabling fault injection: {e}")
        return False, str(e)


def disable_fault_injection(fault_type: Optional[str] = None) -> bool:
    """
    Disable fault injection
    
    Args:
        fault_type: Specific fault type to disable (None = disable all)
        
    Returns:
        bool: True if disabled successfully
    """
    try:
        if FAULT_INJECTOR_AVAILABLE:
            # Use existing fault injector
            result = fi_disable_fault(fault_type)
            
            if result.get('success'):
                with fault_lock:
                    if fault_type is None:
                        # Disable all
                        count = len(fault_state['active_faults'])
                        fault_state['active_faults'] = []
                        fault_state['enabled'] = False
                        fault_stats['active_fault_count'] = 0
                        
                        logger.info(f"Disabled all fault injections ({count} total)")
                    else:
                        # Disable specific fault
                        fault_state['active_faults'] = [
                            f for f in fault_state['active_faults']
                            if f['type'] != fault_type
                        ]
                        fault_stats['active_fault_count'] = len(fault_state['active_faults'])
                        
                        if not fault_state['active_faults']:
                            fault_state['enabled'] = False
                        
                        logger.info(f"Disabled fault injection: {fault_type}")
                
                return True
            else:
                logger.error("Failed to disable fault injection")
                return False
        else:
            # Fallback implementation
            with fault_lock:
                if fault_type is None:
                    # Disable all
                    count = len(fault_state['active_faults'])
                    fault_state['active_faults'] = []
                    fault_state['enabled'] = False
                    fault_stats['active_fault_count'] = 0
                    
                    logger.info(f"Disabled all fault injections (fallback, {count} total)")
                else:
                    # Disable specific fault
                    fault_state['active_faults'] = [
                        f for f in fault_state['active_faults']
                        if f['type'] != fault_type
                    ]
                    fault_stats['active_fault_count'] = len(fault_state['active_faults'])
                    
                    if not fault_state['active_faults']:
                        fault_state['enabled'] = False
                    
                    logger.info(f"Disabled fault injection (fallback): {fault_type}")
            
            return True
        
    except Exception as e:
        logger.error(f"Error disabling fault injection: {e}")
        return False


def get_fault_status() -> Dict:
    """
    Get current fault injection status
    
    Returns:
        dict: Fault injection status
    """
    try:
        if FAULT_INJECTOR_AVAILABLE:
            # Use existing fault injector
            result = fi_get_fault_status()
            
            if result.get('success'):
                return result.get('status', {})
        
        # Fallback or if fault injector doesn't provide detailed status
        with fault_lock:
            status = {
                'enabled': fault_state['enabled'],
                'active_faults': fault_state['active_faults'].copy(),
                'active_fault_count': fault_stats['active_fault_count']
            }
        
        logger.debug(f"Retrieved fault status: {status}")
        return status
        
    except Exception as e:
        logger.error(f"Error getting fault status: {e}")
        return {'error': str(e)}


def get_available_faults() -> List[Dict]:
    """
    Get list of available fault types
    
    Returns:
        list: Available fault types with descriptions
    """
    try:
        if FAULT_INJECTOR_AVAILABLE:
            # Use existing fault injector
            result = fi_get_available_faults()
            
            if result.get('success'):
                return result.get('faults', [])
        
        # Fallback: return common fault types
        available_faults = [
            {
                'type': 'network_delay',
                'description': 'Introduce network latency',
                'parameters': ['delay_ms']
            },
            {
                'type': 'packet_loss',
                'description': 'Drop packets randomly',
                'parameters': ['loss_rate']
            },
            {
                'type': 'memory_exhaustion',
                'description': 'Simulate low memory conditions',
                'parameters': ['threshold_bytes']
            },
            {
                'type': 'cpu_spike',
                'description': 'Simulate high CPU usage',
                'parameters': ['duration_ms']
            },
            {
                'type': 'invalid_data',
                'description': 'Corrupt data packets',
                'parameters': ['corruption_rate']
            }
        ]
        
        logger.debug("Retrieved available faults (fallback list)")
        return available_faults
        
    except Exception as e:
        logger.error(f"Error getting available faults: {e}")
        return []


def get_fault_statistics() -> Dict:
    """
    Get fault injection statistics
    
    Returns:
        dict: Fault statistics
    """
    try:
        with fault_lock:
            stats_copy = fault_stats.copy()
        
        logger.debug(f"Retrieved fault stats: {stats_copy}")
        return stats_copy
        
    except Exception as e:
        logger.error(f"Error getting fault statistics: {e}")
        return {'error': str(e)}


def reset_fault_statistics() -> bool:
    """
    Reset fault injection statistics
    
    Returns:
        bool: True if reset successfully
    """
    try:
        with fault_lock:
            global fault_stats
            fault_stats = {
                'total_faults_enabled': 0,
                'total_faults_triggered': 0,
                'active_fault_count': len(fault_state['active_faults']),
                'last_reset': datetime.now().isoformat()
            }
        
        logger.info("Fault statistics reset")
        return True
        
    except Exception as e:
        logger.error(f"Error resetting fault statistics: {e}")
        return False


def record_fault_trigger(fault_type: str):
    """
    Record that a fault was triggered (for statistics)
    
    Args:
        fault_type: Type of fault that was triggered
    """
    try:
        with fault_lock:
            fault_stats['total_faults_triggered'] += 1
            
            # Add to history
            fault_state['fault_history'].append({
                'type': fault_type,
                'triggered_at': datetime.now().isoformat()
            })
            
            # Keep history limited to last 100 entries
            if len(fault_state['fault_history']) > 100:
                fault_state['fault_history'] = fault_state['fault_history'][-100:]
        
        logger.debug(f"Recorded fault trigger: {fault_type}")
        
    except Exception as e:
        logger.error(f"Error recording fault trigger: {e}")


# Export functions
__all__ = [
    'enable_fault_injection',
    'disable_fault_injection',
    'get_fault_status',
    'get_available_faults',
    'get_fault_statistics',
    'reset_fault_statistics',
    'record_fault_trigger'
]
