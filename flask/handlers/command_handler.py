"""
Command handler for EcoWatt Flask server
Manages command queuing, polling, and result tracking
"""

import logging
import threading
from datetime import datetime
from typing import Dict, List, Optional, Tuple
import uuid

logger = logging.getLogger(__name__)

# Command queue storage (in-memory, can be replaced with Redis/database)
command_queues = {}  # device_id -> list of commands
command_history = {}  # command_id -> command details
command_lock = threading.Lock()

# Command statistics
command_stats = {
    'total_commands': 0,
    'pending_commands': 0,
    'executing_commands': 0,
    'completed_commands': 0,
    'failed_commands': 0,
    'last_reset': datetime.now().isoformat()
}


def queue_command(device_id: str, command: str, parameters: Optional[Dict] = None) -> Tuple[bool, Optional[str]]:
    """
    Queue a command for a device
    
    Args:
        device_id: Device identifier
        command: Command to execute
        parameters: Optional command parameters
        
    Returns:
        tuple: (success, command_id or error_message)
    """
    try:
        # Generate command ID
        command_id = str(uuid.uuid4())
        
        command_data = {
            'command_id': command_id,
            'device_id': device_id,
            'command': command,
            'parameters': parameters or {},
            'status': 'pending',
            'created_at': datetime.now().isoformat()
        }
        
        with command_lock:
            # Initialize queue for device if needed
            if device_id not in command_queues:
                command_queues[device_id] = []
            
            # Add to queue
            command_queues[device_id].append(command_data)
            
            # Add to history
            command_history[command_id] = command_data
            
            # Update statistics
            command_stats['total_commands'] += 1
            command_stats['pending_commands'] += 1
        
        logger.info(f"Command queued for {device_id}: {command} (ID: {command_id})")
        return True, command_id
        
    except Exception as e:
        logger.error(f"Error queueing command: {e}")
        return False, str(e)


def poll_commands(device_id: str, limit: int = 10) -> Tuple[bool, List[Dict]]:
    """
    Poll pending commands for a device
    
    Args:
        device_id: Device identifier
        limit: Maximum number of commands to return
        
    Returns:
        tuple: (success, list of commands)
    """
    try:
        with command_lock:
            if device_id not in command_queues or not command_queues[device_id]:
                return True, []
            
            # Get pending commands
            pending = [cmd for cmd in command_queues[device_id] if cmd['status'] == 'pending']
            commands_to_return = pending[:limit]
            
            # Mark as executing
            for cmd in commands_to_return:
                cmd['status'] = 'executing'
                cmd['polled_at'] = datetime.now().isoformat()
            
            # Update statistics
            if commands_to_return:
                command_stats['pending_commands'] -= len(commands_to_return)
                command_stats['executing_commands'] += len(commands_to_return)
        
        logger.info(f"Polled {len(commands_to_return)} commands for {device_id}")
        return True, commands_to_return
        
    except Exception as e:
        logger.error(f"Error polling commands: {e}")
        return False, []


def submit_command_result(
    device_id: str,
    command_id: str,
    success: bool,
    result_data: Optional[Dict] = None,
    error_message: Optional[str] = None
) -> bool:
    """
    Submit command execution result
    
    Args:
        device_id: Device identifier
        command_id: Command ID
        success: Whether command executed successfully
        result_data: Optional result data
        error_message: Optional error message
        
    Returns:
        bool: True if result submitted successfully
    """
    try:
        with command_lock:
            # Find command in history
            if command_id not in command_history:
                logger.error(f"Command {command_id} not found in history")
                return False
            
            # Update command
            cmd = command_history[command_id]
            cmd['status'] = 'completed' if success else 'failed'
            cmd['completed_at'] = datetime.now().isoformat()
            cmd['result_data'] = result_data or {}
            cmd['error_message'] = error_message
            
            # Remove from queue
            if device_id in command_queues:
                command_queues[device_id] = [
                    c for c in command_queues[device_id]
                    if c['command_id'] != command_id
                ]
            
            # Update statistics
            command_stats['executing_commands'] -= 1
            if success:
                command_stats['completed_commands'] += 1
            else:
                command_stats['failed_commands'] += 1
        
        status = 'successful' if success else 'failed'
        logger.info(f"Command result submitted for {device_id}: {command_id} ({status})")
        return True
        
    except Exception as e:
        logger.error(f"Error submitting command result: {e}")
        return False


def get_command_status(command_id: str) -> Tuple[bool, Optional[Dict]]:
    """
    Get command execution status
    
    Args:
        command_id: Command ID
        
    Returns:
        tuple: (success, status_info)
    """
    try:
        with command_lock:
            if command_id not in command_history:
                logger.warning(f"Command {command_id} not found")
                return False, None
            
            status_info = command_history[command_id].copy()
        
        logger.debug(f"Retrieved status for command {command_id}")
        return True, status_info
        
    except Exception as e:
        logger.error(f"Error getting command status: {e}")
        return False, None


def get_command_history(
    device_id: Optional[str] = None,
    limit: int = 50
) -> Tuple[bool, List[Dict]]:
    """
    Get command execution history
    
    Args:
        device_id: Optional device identifier (None = all devices)
        limit: Maximum number of commands to return
        
    Returns:
        tuple: (success, list of commands)
    """
    try:
        # This would require additional implementation in command_manager
        # For now, return a placeholder
        logger.warning("get_command_history not fully implemented in command_manager")
        
        return True, []
        
    except Exception as e:
        logger.error(f"Error getting command history: {e}")
        return False, []


def get_command_stats() -> Dict:
    """
    Get command statistics
    
    Returns:
        dict: Command statistics
    """
    try:
        with command_lock:
            stats_copy = command_stats.copy()
            
            # Calculate success rate
            total_completed = stats_copy['completed_commands'] + stats_copy['failed_commands']
            if total_completed > 0:
                stats_copy['success_rate'] = round(
                    (stats_copy['completed_commands'] / total_completed) * 100, 2
                )
            else:
                stats_copy['success_rate'] = 0.0
        
        logger.debug(f"Retrieved command stats: {stats_copy}")
        return stats_copy
        
    except Exception as e:
        logger.error(f"Error getting command stats: {e}")
        return {'error': str(e)}


def reset_command_stats() -> bool:
    """
    Reset command statistics
    
    Returns:
        bool: True if reset successfully
    """
    try:
        with command_lock:
            global command_stats
            command_stats = {
                'total_commands': 0,
                'pending_commands': 0,
                'executing_commands': 0,
                'completed_commands': 0,
                'failed_commands': 0,
                'last_reset': datetime.now().isoformat()
            }
        
        logger.info("Command statistics reset")
        return True
        
    except Exception as e:
        logger.error(f"Error resetting command stats: {e}")
        return False


def cancel_pending_commands(device_id: str) -> Tuple[bool, int]:
    """
    Cancel all pending commands for a device
    
    Args:
        device_id: Device identifier
        
    Returns:
        tuple: (success, number of cancelled commands)
    """
    try:
        # This would require additional implementation in command_manager
        # For now, return a placeholder
        logger.warning("cancel_pending_commands not fully implemented in command_manager")
        
        return True, 0
        
    except Exception as e:
        logger.error(f"Error cancelling pending commands: {e}")
        return False, 0


# Export functions
__all__ = [
    'queue_command',
    'poll_commands',
    'submit_command_result',
    'get_command_status',
    'get_command_history',
    'get_command_stats',
    'reset_command_stats',
    'cancel_pending_commands'
]
