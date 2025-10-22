"""
Logging utilities for EcoWatt Flask server
Provides centralized logging configuration
"""

import logging
import sys
from datetime import datetime
from pathlib import Path


def init_logging(
    level=logging.INFO,
    log_file=None,
    log_format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    date_format='%Y-%m-%d %H:%M:%S'
):
    """
    Initialize logging configuration for the application
    
    Args:
        level: Logging level (logging.DEBUG, INFO, WARNING, ERROR, CRITICAL)
        log_file: Optional path to log file (None = console only)
        log_format: Log message format string
        date_format: Date/time format string
        
    Returns:
        logging.Logger: Root logger instance
    """
    # Create formatter
    formatter = logging.Formatter(log_format, datefmt=date_format)
    
    # Get root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(level)
    
    # Remove existing handlers
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)
    
    # Console handler (stdout)
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(level)
    console_handler.setFormatter(formatter)
    root_logger.addHandler(console_handler)
    
    # File handler (if specified)
    if log_file:
        # Create log directory if it doesn't exist
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        
        file_handler = logging.FileHandler(log_file, mode='a')
        file_handler.setLevel(level)
        file_handler.setFormatter(formatter)
        root_logger.addHandler(file_handler)
        
        root_logger.info(f"Logging to file: {log_file}")
    
    root_logger.info("Logging initialized")
    return root_logger


def get_logger(name, level=None):
    """
    Get a logger instance for a specific module
    
    Args:
        name: Logger name (typically __name__)
        level: Optional logging level (inherits from root if not specified)
        
    Returns:
        logging.Logger: Logger instance
    """
    logger = logging.getLogger(name)
    
    if level is not None:
        logger.setLevel(level)
    
    return logger


def set_log_level(level, logger_name=None):
    """
    Change logging level at runtime
    
    Args:
        level: New logging level (logging.DEBUG, INFO, WARNING, ERROR, CRITICAL)
        logger_name: Optional logger name (None = root logger)
    """
    if logger_name:
        logger = logging.getLogger(logger_name)
    else:
        logger = logging.getLogger()
    
    logger.setLevel(level)
    
    # Update all handlers
    for handler in logger.handlers:
        handler.setLevel(level)
    
    logger.info(f"Log level changed to: {logging.getLevelName(level)}")


def log_function_call(func):
    """
    Decorator to log function calls
    
    Usage:
        @log_function_call
        def my_function(arg1, arg2):
            pass
    
    Args:
        func: Function to wrap
        
    Returns:
        Wrapped function
    """
    def wrapper(*args, **kwargs):
        logger = logging.getLogger(func.__module__)
        logger.debug(f"Calling {func.__name__}({args}, {kwargs})")
        try:
            result = func(*args, **kwargs)
            logger.debug(f"{func.__name__} returned: {result}")
            return result
        except Exception as e:
            logger.error(f"{func.__name__} raised exception: {e}")
            raise
    
    wrapper.__name__ = func.__name__
    wrapper.__doc__ = func.__doc__
    return wrapper


def create_audit_logger(log_file='security_audit.log'):
    """
    Create a dedicated logger for security audit events
    
    Args:
        log_file: Path to audit log file
        
    Returns:
        logging.Logger: Audit logger instance
    """
    audit_logger = logging.getLogger('security_audit')
    audit_logger.setLevel(logging.INFO)
    audit_logger.propagate = False  # Don't propagate to root logger
    
    # Create log directory if needed
    log_path = Path(log_file)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    
    # File handler with specific format for audit
    file_handler = logging.FileHandler(log_file, mode='a')
    file_handler.setLevel(logging.INFO)
    
    audit_format = '%(asctime)s - AUDIT - %(message)s'
    formatter = logging.Formatter(audit_format, datefmt='%Y-%m-%d %H:%M:%S')
    file_handler.setFormatter(formatter)
    
    audit_logger.addHandler(file_handler)
    
    return audit_logger


def log_security_event(event_type, device_id, details):
    """
    Log a security event to audit log
    
    Event types:
    - REPLAY_BLOCKED: Duplicate nonce detected
    - TAMPER_DETECTED: Invalid signature/MAC
    - MAC_INVALID: MAC verification failed
    - VALID_REQUEST: Successful authentication
    - NONCE_EXPIRED: Old nonce rejected
    
    Args:
        event_type: Type of security event
        device_id: Device identifier
        details: Additional details (dict or string)
    """
    audit_logger = logging.getLogger('security_audit')
    
    if isinstance(details, dict):
        details_str = ', '.join(f"{k}={v}" for k, v in details.items())
    else:
        details_str = str(details)
    
    message = f"[{event_type}] Device: {device_id} - {details_str}"
    audit_logger.info(message)


def enable_debug_logging():
    """
    Enable debug logging for all loggers
    """
    set_log_level(logging.DEBUG)
    logging.info("Debug logging enabled")


def disable_debug_logging():
    """
    Disable debug logging (set to INFO)
    """
    set_log_level(logging.INFO)
    logging.info("Debug logging disabled")


# Export functions
__all__ = [
    'init_logging',
    'get_logger',
    'set_log_level',
    'log_function_call',
    'create_audit_logger',
    'log_security_event',
    'enable_debug_logging',
    'disable_debug_logging'
]
