# Command Manager for EcoWatt Cloud
# Handles command queuing, execution tracking, and logging

import json
import time
from datetime import datetime
from typing import Dict, List, Optional
import threading
import logging

logger = logging.getLogger(__name__)

class CommandManager:
    """Manages command queue and execution tracking for EcoWatt devices."""
    
    def __init__(self, log_file='commands.log'):
        self.command_queue: Dict[str, List[Dict]] = {}  # device_id -> list of commands
        self.command_history: List[Dict] = []  # All commands history
        self.pending_results: Dict[str, Dict] = {}  # command_id -> result waiting for
        self.lock = threading.Lock()
        self.log_file = log_file
        self.command_counter = 1000  # Start from 1000 for command IDs
        
        logger.info("Command Manager initialized")
    
    def queue_command(self, device_id: str, command_type: str, parameters: Dict) -> str:
        """
        Queue a new command for a device.
        
        Args:
            device_id: Target device identifier
            command_type: Type of command (e.g., 'set_power', 'write_register')
            parameters: Command parameters
            
        Returns:
            command_id: Unique identifier for this command
        """
        with self.lock:
            # Generate unique command ID
            command_id = f"CMD_{self.command_counter}_{int(time.time())}"
            self.command_counter += 1
            
            # Create command object
            command = {
                'command_id': command_id,
                'device_id': device_id,
                'command_type': command_type,
                'parameters': parameters,
                'status': 'queued',
                'queued_time': datetime.now().isoformat(),
                'queued_timestamp': int(time.time() * 1000),
                'sent_time': None,
                'completed_time': None,
                'result': None,
                'error': None
            }
            
            # Add to device queue
            if device_id not in self.command_queue:
                self.command_queue[device_id] = []
            
            self.command_queue[device_id].append(command)
            
            # Add to history
            self.command_history.append(command.copy())
            
            # Log to file
            self._log_command_event(command, 'QUEUED')
            
            logger.info(f"Command queued: {command_id} for {device_id} - Type: {command_type}")
            
            return command_id
    
    def get_next_command(self, device_id: str) -> Optional[Dict]:
        """
        Get the next pending command for a device.
        
        Args:
            device_id: Device requesting commands
            
        Returns:
            command dict or None if no pending commands
        """
        with self.lock:
            if device_id not in self.command_queue or not self.command_queue[device_id]:
                logger.debug(f"No pending commands for {device_id}")
                return None
            
            # Get first queued command
            for cmd in self.command_queue[device_id]:
                if cmd['status'] == 'queued':
                    # Mark as sent
                    cmd['status'] = 'sent'
                    cmd['sent_time'] = datetime.now().isoformat()
                    
                    # Update history
                    self._update_history(cmd['command_id'], cmd)
                    
                    # Log event
                    self._log_command_event(cmd, 'SENT')
                    
                    logger.info(f"Command sent to {device_id}: {cmd['command_id']}")
                    
                    return {
                        'command_id': cmd['command_id'],
                        'command_type': cmd['command_type'],
                        'parameters': cmd['parameters']
                    }
            
            logger.debug(f"No queued commands for {device_id} (all already sent)")
            return None
    
    def record_result(self, command_id: str, status: str, result: str, device_id: str = None) -> bool:
        """
        Record the execution result of a command.
        
        Args:
            command_id: Command identifier
            status: 'completed' or 'failed'
            result: Result message or error description
            device_id: Optional device ID for lookup
            
        Returns:
            True if command was found and updated, False otherwise
        """
        with self.lock:
            # Find command in queues
            command = None
            target_device = device_id
            
            if device_id and device_id in self.command_queue:
                for cmd in self.command_queue[device_id]:
                    if cmd['command_id'] == command_id:
                        command = cmd
                        break
            else:
                # Search all devices
                for dev_id, queue in self.command_queue.items():
                    for cmd in queue:
                        if cmd['command_id'] == command_id:
                            command = cmd
                            target_device = dev_id
                            break
                    if command:
                        break
            
            if not command:
                logger.warning(f"Command not found: {command_id}")
                return False
            
            # Update command
            command['status'] = status
            command['completed_time'] = datetime.now().isoformat()
            command['result'] = result
            
            # Update history
            self._update_history(command_id, command)
            
            # Log event
            self._log_command_event(command, status.upper())
            
            logger.info(f"Command {command_id} {status}: {result}")
            
            return True
    
    def get_command_status(self, command_id: str) -> Optional[Dict]:
        """Get status of a specific command."""
        with self.lock:
            for cmd in self.command_history:
                if cmd['command_id'] == command_id:
                    return cmd.copy()
            return None
    
    def get_device_commands(self, device_id: str, limit: int = 50) -> List[Dict]:
        """Get command history for a specific device."""
        with self.lock:
            device_cmds = [cmd for cmd in self.command_history 
                          if cmd['device_id'] == device_id]
            # Return most recent first
            return sorted(device_cmds, 
                         key=lambda x: x['queued_timestamp'], 
                         reverse=True)[:limit]
    
    def get_all_commands(self, limit: int = 100) -> List[Dict]:
        """Get all command history."""
        with self.lock:
            return sorted(self.command_history, 
                         key=lambda x: x['queued_timestamp'], 
                         reverse=True)[:limit]
    
    def get_pending_commands(self, device_id: str = None) -> List[Dict]:
        """Get all pending (queued or sent) commands."""
        with self.lock:
            pending = []
            
            if device_id:
                if device_id in self.command_queue:
                    pending = [cmd for cmd in self.command_queue[device_id]
                              if cmd['status'] in ['queued', 'sent']]
            else:
                for queue in self.command_queue.values():
                    pending.extend([cmd for cmd in queue 
                                   if cmd['status'] in ['queued', 'sent']])
            
            return pending
    
    def clear_completed_commands(self, device_id: str = None, older_than_hours: int = 24):
        """Remove completed commands older than specified hours."""
        with self.lock:
            cutoff_time = time.time() - (older_than_hours * 3600)
            
            if device_id:
                if device_id in self.command_queue:
                    self.command_queue[device_id] = [
                        cmd for cmd in self.command_queue[device_id]
                        if cmd['status'] in ['queued', 'sent'] or 
                        cmd['queued_timestamp'] / 1000 > cutoff_time
                    ]
            else:
                for dev_id in self.command_queue:
                    self.command_queue[dev_id] = [
                        cmd for cmd in self.command_queue[dev_id]
                        if cmd['status'] in ['queued', 'sent'] or 
                        cmd['queued_timestamp'] / 1000 > cutoff_time
                    ]
            
            logger.info(f"Cleared completed commands older than {older_than_hours} hours")
    
    def get_statistics(self) -> Dict:
        """Get command statistics."""
        with self.lock:
            total = len(self.command_history)
            completed = sum(1 for cmd in self.command_history if cmd['status'] == 'completed')
            failed = sum(1 for cmd in self.command_history if cmd['status'] == 'failed')
            pending = sum(1 for cmd in self.command_history if cmd['status'] in ['queued', 'sent'])
            
            return {
                'total_commands': total,
                'completed': completed,
                'failed': failed,
                'pending': pending,
                'success_rate': round(completed / total * 100, 2) if total > 0 else 0,
                'active_devices': len(self.command_queue),
                'total_queued': sum(len(q) for q in self.command_queue.values())
            }
    
    def _update_history(self, command_id: str, updated_command: Dict):
        """Update command in history."""
        for i, cmd in enumerate(self.command_history):
            if cmd['command_id'] == command_id:
                self.command_history[i] = updated_command.copy()
                break
    
    def _log_command_event(self, command: Dict, event_type: str):
        """Log command event to file."""
        try:
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'event': event_type,
                'command_id': command['command_id'],
                'device_id': command['device_id'],
                'command_type': command['command_type'],
                'parameters': command['parameters'],
                'status': command['status'],
                'result': command.get('result')
            }
            
            with open(self.log_file, 'a') as f:
                f.write(json.dumps(log_entry) + '\n')
                
        except Exception as e:
            logger.error(f"Failed to write command log: {e}")
    
    def export_logs(self, device_id: str = None, start_time: str = None, end_time: str = None) -> List[Dict]:
        """Export command logs with optional filtering."""
        try:
            logs = []
            with open(self.log_file, 'r') as f:
                for line in f:
                    try:
                        log = json.loads(line.strip())
                        
                        # Apply filters
                        if device_id and log.get('device_id') != device_id:
                            continue
                        
                        if start_time and log.get('timestamp', '') < start_time:
                            continue
                        
                        if end_time and log.get('timestamp', '') > end_time:
                            continue
                        
                        logs.append(log)
                    except json.JSONDecodeError:
                        continue
            
            return logs
            
        except FileNotFoundError:
            logger.warning(f"Log file {self.log_file} not found")
            return []
        except Exception as e:
            logger.error(f"Error reading logs: {e}")
            return []
