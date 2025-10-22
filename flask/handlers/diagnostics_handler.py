"""
Diagnostics handler for EcoWatt Flask server
Manages storage and retrieval of diagnostic data
"""

import logging
import json
import threading
from datetime import datetime
from typing import Dict, List, Optional
from pathlib import Path

logger = logging.getLogger(__name__)

# In-memory diagnostics storage (can be replaced with database later)
diagnostics_store = {}
diagnostics_lock = threading.Lock()

# Persistent storage configuration
DIAGNOSTICS_DIR = Path('diagnostics_data')
DIAGNOSTICS_DIR.mkdir(exist_ok=True)


def store_diagnostics(device_id: str, diagnostics_data: Dict) -> bool:
    """
    Store diagnostic data for a device
    
    Args:
        device_id: Unique device identifier
        diagnostics_data: Diagnostic data dictionary
        
    Returns:
        bool: True if stored successfully, False otherwise
    """
    try:
        # Add timestamp if not present
        if 'timestamp' not in diagnostics_data:
            diagnostics_data['timestamp'] = datetime.now().isoformat()
        
        # Add device_id to data
        diagnostics_data['device_id'] = device_id
        
        with diagnostics_lock:
            # Initialize device storage if needed
            if device_id not in diagnostics_store:
                diagnostics_store[device_id] = []
            
            # Append diagnostics
            diagnostics_store[device_id].append(diagnostics_data)
            
            # Keep only last 100 entries per device
            if len(diagnostics_store[device_id]) > 100:
                diagnostics_store[device_id] = diagnostics_store[device_id][-100:]
            
            total_entries = len(diagnostics_store[device_id])
        
        # Persist to file (optional, async in production)
        _persist_diagnostics(device_id, diagnostics_data)
        
        logger.info(f"Stored diagnostics for {device_id}: {total_entries} total entries")
        return True
        
    except Exception as e:
        logger.error(f"Error storing diagnostics for {device_id}: {e}")
        return False


def get_diagnostics_by_device(device_id: str, limit: int = 10) -> List[Dict]:
    """
    Retrieve diagnostic data for a specific device
    
    Args:
        device_id: Device identifier
        limit: Maximum number of entries to return (default: 10)
        
    Returns:
        list: List of diagnostic entries (most recent first)
    """
    try:
        with diagnostics_lock:
            if device_id not in diagnostics_store:
                logger.warning(f"No diagnostics found for device: {device_id}")
                return []
            
            # Return most recent entries
            entries = diagnostics_store[device_id][-limit:]
            entries.reverse()  # Most recent first
            
        logger.info(f"Retrieved {len(entries)} diagnostics for {device_id}")
        return entries
        
    except Exception as e:
        logger.error(f"Error retrieving diagnostics for {device_id}: {e}")
        return []


def get_all_diagnostics(limit_per_device: int = 10) -> Dict[str, List[Dict]]:
    """
    Retrieve diagnostic data for all devices
    
    Args:
        limit_per_device: Maximum entries per device (default: 10)
        
    Returns:
        dict: Dictionary mapping device_id to list of diagnostics
    """
    try:
        with diagnostics_lock:
            all_diagnostics = {}
            
            for device_id in diagnostics_store.keys():
                entries = diagnostics_store[device_id][-limit_per_device:]
                entries.reverse()  # Most recent first
                all_diagnostics[device_id] = entries
        
        total_devices = len(all_diagnostics)
        total_entries = sum(len(entries) for entries in all_diagnostics.values())
        
        logger.info(f"Retrieved diagnostics for {total_devices} devices, "
                   f"{total_entries} total entries")
        return all_diagnostics
        
    except Exception as e:
        logger.error(f"Error retrieving all diagnostics: {e}")
        return {}


def clear_diagnostics(device_id: Optional[str] = None) -> bool:
    """
    Clear diagnostic data for a device or all devices
    
    Args:
        device_id: Device identifier (None = clear all)
        
    Returns:
        bool: True if cleared successfully
    """
    try:
        with diagnostics_lock:
            if device_id is None:
                # Clear all diagnostics
                count = len(diagnostics_store)
                diagnostics_store.clear()
                logger.info(f"Cleared diagnostics for all {count} devices")
            else:
                # Clear specific device
                if device_id in diagnostics_store:
                    del diagnostics_store[device_id]
                    logger.info(f"Cleared diagnostics for device: {device_id}")
                else:
                    logger.warning(f"No diagnostics found for device: {device_id}")
        
        return True
        
    except Exception as e:
        logger.error(f"Error clearing diagnostics: {e}")
        return False


def get_diagnostics_summary(device_id: Optional[str] = None) -> Dict:
    """
    Get summary statistics for diagnostics
    
    Args:
        device_id: Device identifier (None = all devices)
        
    Returns:
        dict: Summary statistics
    """
    try:
        with diagnostics_lock:
            if device_id:
                # Summary for specific device
                if device_id not in diagnostics_store:
                    return {'error': 'Device not found'}
                
                entries = diagnostics_store[device_id]
                return {
                    'device_id': device_id,
                    'total_entries': len(entries),
                    'first_seen': entries[0]['timestamp'] if entries else None,
                    'last_seen': entries[-1]['timestamp'] if entries else None
                }
            else:
                # Summary for all devices
                total_devices = len(diagnostics_store)
                total_entries = sum(len(entries) for entries in diagnostics_store.values())
                
                device_summaries = {}
                for dev_id, entries in diagnostics_store.items():
                    device_summaries[dev_id] = {
                        'entry_count': len(entries),
                        'last_seen': entries[-1]['timestamp'] if entries else None
                    }
                
                return {
                    'total_devices': total_devices,
                    'total_entries': total_entries,
                    'devices': device_summaries
                }
        
    except Exception as e:
        logger.error(f"Error getting diagnostics summary: {e}")
        return {'error': str(e)}


def _persist_diagnostics(device_id: str, diagnostics_data: Dict):
    """
    Persist diagnostics to file (internal function)
    
    Args:
        device_id: Device identifier
        diagnostics_data: Data to persist
    """
    try:
        # Create device-specific file
        file_path = DIAGNOSTICS_DIR / f"{device_id}_diagnostics.jsonl"
        
        # Append as JSON lines
        with open(file_path, 'a') as f:
            json.dump(diagnostics_data, f)
            f.write('\n')
        
        logger.debug(f"Persisted diagnostics to {file_path}")
        
    except Exception as e:
        logger.warning(f"Failed to persist diagnostics to file: {e}")


def load_persisted_diagnostics(device_id: str) -> List[Dict]:
    """
    Load persisted diagnostics from file
    
    Args:
        device_id: Device identifier
        
    Returns:
        list: List of diagnostic entries
    """
    try:
        file_path = DIAGNOSTICS_DIR / f"{device_id}_diagnostics.jsonl"
        
        if not file_path.exists():
            logger.debug(f"No persisted diagnostics file for {device_id}")
            return []
        
        diagnostics = []
        with open(file_path, 'r') as f:
            for line in f:
                if line.strip():
                    diagnostics.append(json.loads(line))
        
        logger.info(f"Loaded {len(diagnostics)} persisted diagnostics for {device_id}")
        return diagnostics
        
    except Exception as e:
        logger.error(f"Error loading persisted diagnostics: {e}")
        return []


# Export functions
__all__ = [
    'store_diagnostics',
    'get_diagnostics_by_device',
    'get_all_diagnostics',
    'clear_diagnostics',
    'get_diagnostics_summary',
    'load_persisted_diagnostics'
]
