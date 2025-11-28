"""
Logging utilities for EcoWatt Flask server
Provides centralized logging configuration with ✓/✗ symbols
"""

import logging
import sys
import uuid
from datetime import datetime
from pathlib import Path
from contextvars import ContextVar

# Context variable to store request ID for each request
request_id_ctx = ContextVar('request_id', default=None)


class ColoredFormatter(logging.Formatter):
    """Custom formatter with colors and symbols for console output"""
    
    # ANSI color codes
    COLORS = {
        'DEBUG': '\033[36m',    # Cyan
        'INFO': '\033[37m',     # White
        'WARNING': '\033[33m',  # Yellow
        'ERROR': '\033[31m',    # Red
        'CRITICAL': '\033[35m', # Magenta
        'RESET': '\033[0m',     # Reset
        'BOLD': '\033[1m',      # Bold
    }
    
    # Symbols for different log levels
    SYMBOLS = {
        'DEBUG': '    ',
        'INFO': '    ',
        'WARNING': '[!] ',
        'ERROR': '✗   ',
        'CRITICAL': '✗✗  ',
        'SUCCESS': '✓   ',
    }
    
    def format(self, record):
        # Add request ID if available
        request_id = request_id_ctx.get()
        if request_id:
            record.request_id = f"[{request_id[:8]}]"
        else:
            record.request_id = ""
        
        # Add symbol based on level
        symbol = self.SYMBOLS.get(record.levelname, '    ')
        
        # Check if it's a success message
        if hasattr(record, 'is_success') and record.is_success:
            symbol = self.SYMBOLS['SUCCESS']
            color = self.COLORS['RESET']
        else:
            color = self.COLORS.get(record.levelname, self.COLORS['RESET'])
        
        # Format the message with timestamp
        log_fmt = f"{color}[%(asctime)s] [%(name)-15s] {symbol}%(message)s{self.COLORS['RESET']}"
        
        if request_id:
            log_fmt = f"{color}[%(asctime)s] [%(name)-15s] %(request_id)s {symbol}%(message)s{self.COLORS['RESET']}"
        
        formatter = logging.Formatter(log_fmt, datefmt='%H:%M:%S')
        return formatter.format(record)


class FileFormatter(logging.Formatter):
    """Formatter for file output (no colors, plain text)"""
    
    def format(self, record):
        # Add request ID if available
        request_id = request_id_ctx.get()
        if request_id:
            record.request_id = f"[{request_id}]"
        else:
            record.request_id = ""
        
        # Determine symbol
        if hasattr(record, 'is_success') and record.is_success:
            symbol = '✓'
        elif record.levelname == 'ERROR':
            symbol = '✗'
        elif record.levelname == 'WARNING':
            symbol = '!'
        else:
            symbol = ' '
        
        if request_id:
            log_fmt = f"[%(asctime)s] [%(levelname)-8s] [%(name)-15s] %(request_id)s [{symbol}] %(message)s"
        else:
            log_fmt = f"[%(asctime)s] [%(levelname)-8s] [%(name)-15s] [{symbol}] %(message)s"
        
        formatter = logging.Formatter(log_fmt, datefmt='%Y-%m-%d %H:%M:%S')
        return formatter.format(record)


def init_logging(
    level=logging.INFO,
    log_file='logs/ecowatt_server.log',
    console_colors=True,
    session_based=True
):
    """
    Initialize logging configuration for the application
    
    Args:
        level: Logging level (logging.DEBUG, INFO, WARNING, ERROR, CRITICAL)
        log_file: Path to log file (None = console only, default: logs/ecowatt_server.log)
        console_colors: Enable colored console output (default: True)
        session_based: Create new log file for each session with timestamp (default: True)
        
    Returns:
        logging.Logger: Root logger instance
    """
    # Get root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(level)
    
    # Remove existing handlers
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)
    
    # Console handler (stdout) with colors
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(level)
    
    if console_colors:
        console_handler.setFormatter(ColoredFormatter())
    else:
        console_handler.setFormatter(FileFormatter())
    
    root_logger.addHandler(console_handler)
    
    # File handler (if specified)
    if log_file:
        # Create log directory if it doesn't exist
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Generate session-based filename if enabled
        if session_based:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            base_name = log_path.stem  # e.g., 'ecowatt_server'
            extension = log_path.suffix  # e.g., '.log'
            session_log_file = log_path.parent / f"{base_name}_{timestamp}{extension}"
        else:
            session_log_file = log_path
        
        file_handler = logging.FileHandler(session_log_file, mode='w')  # 'w' for new session
        file_handler.setLevel(level)
        file_handler.setFormatter(FileFormatter())
        root_logger.addHandler(file_handler)
        
        root_logger.info(f"Logging to file: {session_log_file}")
    
    root_logger.info("Logging system initialized")
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


def log_success(logger, message, *args, **kwargs):
    """
    Log a success message with ✓ symbol
    
    Args:
        logger: Logger instance
        message: Message to log
        *args: Format arguments
        **kwargs: Additional logging kwargs
    """
    # Create log record with success flag
    if args:
        message = message % args
    
    record = logger.makeRecord(
        logger.name,
        logging.INFO,
        "(unknown file)",
        0,
        message,
        (),
        None
    )
    record.is_success = True
    logger.handle(record)


def set_request_id(request_id=None):
    """
    Set request ID for current context
    
    Args:
        request_id: Request ID (generates new UUID if None)
        
    Returns:
        str: Request ID
    """
    if request_id is None:
        request_id = str(uuid.uuid4())
    request_id_ctx.set(request_id)
    return request_id


def get_request_id():
    """
    Get current request ID
    
    Returns:
        str: Request ID or None
    """
    return request_id_ctx.get()


def clear_request_id():
    """Clear request ID from context"""
    request_id_ctx.set(None)


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
    'log_success',
    'set_request_id',
    'get_request_id',
    'clear_request_id',
    'enable_debug_logging',
    'disable_debug_logging',
    'ColoredFormatter',
    'FileFormatter'
]
