"""
Database handler for EcoWatt Flask Server
Provides persistent storage for sensor data, commands, configs, and OTA updates
Data is kept indefinitely - manual cleanup available if needed
"""

import sqlite3
import json
import logging
from datetime import datetime, timedelta, timezone
from pathlib import Path
import threading
from typing import Dict, List, Optional, Any
import pytz
from utils.logger_utils import log_success

# Logger
logger = logging.getLogger(__name__)

# Thread-safe database connection pool
_thread_local = threading.local()

# Database configuration
DB_PATH = Path(__file__).parent / 'ecowatt_data.db'
# RETENTION_DAYS = None means keep data forever
# Change this to a number (e.g., 365) if you want automatic cleanup after X days
RETENTION_DAYS = None

# Timezone for Sri Lanka
SRI_LANKA_TZ = pytz.timezone('Asia/Colombo')

def convert_utc_to_local(utc_timestamp_str):
    """Convert UTC timestamp string from SQLite to Sri Lanka local time ISO format"""
    if not utc_timestamp_str:
        return None
    try:
        # Parse the UTC timestamp from SQLite
        # Try different formats that SQLite might use
        utc_dt = None
        
        # Try with timezone offset first (format: YYYY-MM-DD HH:MM:SS+00:00)
        try:
            # Remove timezone suffix if present and parse manually
            if '+' in utc_timestamp_str or utc_timestamp_str.endswith('Z'):
                # Strip timezone info - we know it's UTC
                base_timestamp = utc_timestamp_str.split('+')[0].split('Z')[0]
                if '.' in base_timestamp:
                    utc_dt = datetime.strptime(base_timestamp, '%Y-%m-%d %H:%M:%S.%f')
                else:
                    utc_dt = datetime.strptime(base_timestamp, '%Y-%m-%d %H:%M:%S')
                # Already UTC, just add timezone info
                utc_dt = utc_dt.replace(tzinfo=timezone.utc)
        except ValueError:
            pass
        
        # Fallback: Try with microseconds (format: YYYY-MM-DD HH:MM:SS.ffffff)
        if utc_dt is None:
            try:
                utc_dt = datetime.strptime(utc_timestamp_str, '%Y-%m-%d %H:%M:%S.%f')
                utc_dt = utc_dt.replace(tzinfo=timezone.utc)
            except ValueError:
                # Fallback to format without microseconds (format: YYYY-MM-DD HH:MM:SS)
                utc_dt = datetime.strptime(utc_timestamp_str, '%Y-%m-%d %H:%M:%S')
                utc_dt = utc_dt.replace(tzinfo=timezone.utc)
        
        # Convert to Sri Lanka time
        local_dt = utc_dt.astimezone(SRI_LANKA_TZ)
        # Return as ISO format string
        return local_dt.isoformat()
    except Exception as e:
        logger.warning(f"Failed to convert timestamp {utc_timestamp_str}: {e}")
        return utc_timestamp_str

class Database:
    """Thread-safe SQLite database handler"""
    
    @staticmethod
    def get_connection():
        """Get thread-local database connection"""
        if not hasattr(_thread_local, 'connection'):
            _thread_local.connection = sqlite3.connect(str(DB_PATH), check_same_thread=False)
            _thread_local.connection.row_factory = sqlite3.Row  # Return dict-like rows
        return _thread_local.connection
    
    @staticmethod
    def init_database():
        """Initialize database schema"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # Sensor data table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                timestamp DATETIME NOT NULL,
                register_data TEXT NOT NULL,
                compression_method TEXT,
                compression_ratio REAL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_timestamp ON sensor_data(device_id, timestamp)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_created_at ON sensor_data(created_at)')
        
        # Devices table - persistent device registry
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS devices (
                device_id TEXT PRIMARY KEY,
                device_name TEXT NOT NULL,
                location TEXT,
                description TEXT,
                status TEXT DEFAULT 'active',
                firmware_version TEXT,
                registered_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_seen DATETIME
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_status ON devices(status)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_last_seen ON devices(last_seen)')
        
        # Commands table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS commands (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                command_id TEXT UNIQUE NOT NULL,
                device_id TEXT NOT NULL,
                command TEXT NOT NULL,
                status TEXT DEFAULT 'pending',
                result TEXT,
                error_msg TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                acknowledged_at DATETIME
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_status_cmd ON commands(device_id, status)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_command_id ON commands(command_id)')
        
        # Configurations table (only ONE pending per device)
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS configurations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                config TEXT NOT NULL,
                status TEXT DEFAULT 'pending',
                error_msg TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                acknowledged_at DATETIME
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_status_cfg ON configurations(device_id, status)')
        
        # OTA updates table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS ota_updates (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                from_version TEXT,
                to_version TEXT NOT NULL,
                status TEXT DEFAULT 'initiated',
                download_status TEXT,
                install_status TEXT,
                error_msg TEXT,
                firmware_size INTEGER,
                chunks_total INTEGER,
                chunks_downloaded INTEGER,
                initiated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                download_completed_at DATETIME,
                install_completed_at DATETIME
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_status_ota ON ota_updates(device_id, status)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_initiated_at ON ota_updates(initiated_at)')
        
        # Power reports table - stores energy consumption and power management statistics
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS power_reports (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                timestamp DATETIME NOT NULL,
                enabled INTEGER NOT NULL,
                techniques INTEGER NOT NULL,
                avg_current_ma REAL NOT NULL,
                energy_saved_mah REAL NOT NULL,
                uptime_ms INTEGER NOT NULL,
                high_perf_ms INTEGER DEFAULT 0,
                normal_ms INTEGER DEFAULT 0,
                low_power_ms INTEGER DEFAULT 0,
                sleep_ms INTEGER DEFAULT 0,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_timestamp_power ON power_reports(device_id, timestamp)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_created_at_power ON power_reports(created_at)')
        
        # Fault injection history table - stores fault injection requests
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS fault_injections (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT,
                fault_type TEXT NOT NULL,
                backend TEXT NOT NULL,
                error_type TEXT,
                exception_code INTEGER,
                delay_ms INTEGER,
                success INTEGER NOT NULL,
                error_msg TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_fault_inj ON fault_injections(device_id, created_at)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_fault_type ON fault_injections(fault_type)')
        
        # Fault recovery events table - stores ESP32 recovery events
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS fault_recovery_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                fault_type TEXT NOT NULL,
                recovery_action TEXT NOT NULL,
                success INTEGER NOT NULL,
                details TEXT,
                retry_count INTEGER,
                received_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_device_recovery ON fault_recovery_events(device_id, received_at)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_recovery_success ON fault_recovery_events(success)')
        
        conn.commit()
        log_success(logger, "Database schema initialized successfully")
        
        # Run initial cleanup (only if retention period is set)
        if RETENTION_DAYS is not None:
            Database.cleanup_old_data()
            logger.info(f"Auto-cleanup enabled with {RETENTION_DAYS}-day retention")
        else:
            logger.info("Data retention: UNLIMITED - data will be kept forever")
    
    # ============================================
    # SENSOR DATA OPERATIONS
    # ============================================
    
    @staticmethod
    def save_sensor_data(device_id: str, timestamp: datetime, register_data: Dict, 
                        compression_method: str = None, compression_ratio: float = None) -> int:
        """Save sensor data to database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO sensor_data (device_id, timestamp, register_data, compression_method, compression_ratio)
            VALUES (?, ?, ?, ?, ?)
        ''', (device_id, timestamp, json.dumps(register_data), compression_method, compression_ratio))
        
        conn.commit()
        return cursor.lastrowid
    
    @staticmethod
    def get_latest_sensor_data(device_id: str) -> Optional[Dict]:
        """Get latest sensor data for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM sensor_data 
            WHERE device_id = ? 
            ORDER BY timestamp DESC 
            LIMIT 1
        ''', (device_id,))
        
        row = cursor.fetchone()
        if row:
            return {
                'id': row['id'],
                'device_id': row['device_id'],
                'timestamp': row['timestamp'],
                'register_data': json.loads(row['register_data']),
                'compression_method': row['compression_method'],
                'compression_ratio': row['compression_ratio'],
                'created_at': row['created_at']
            }
        return None
    
    @staticmethod
    def get_historical_sensor_data(device_id: str, start_time: datetime = None, 
                                   end_time: datetime = None, limit: int = 1000) -> List[Dict]:
        """Get historical sensor data for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        if start_time and end_time:
            cursor.execute('''
                SELECT * FROM sensor_data 
                WHERE device_id = ? AND timestamp BETWEEN ? AND ?
                ORDER BY timestamp DESC 
                LIMIT ?
            ''', (device_id, start_time, end_time, limit))
        elif start_time:
            cursor.execute('''
                SELECT * FROM sensor_data 
                WHERE device_id = ? AND timestamp >= ?
                ORDER BY timestamp DESC 
                LIMIT ?
            ''', (device_id, start_time, limit))
        else:
            cursor.execute('''
                SELECT * FROM sensor_data 
                WHERE device_id = ? 
                ORDER BY timestamp DESC 
                LIMIT ?
            ''', (device_id, limit))
        
        rows = cursor.fetchall()
        return [{
            'id': row['id'],
            'device_id': row['device_id'],
            'timestamp': row['timestamp'],
            'register_data': json.loads(row['register_data']),
            'compression_method': row['compression_method'],
            'compression_ratio': row['compression_ratio'],
            'created_at': row['created_at']
        } for row in rows]
    
    # ============================================
    # DEVICE OPERATIONS
    # ============================================
    
    @staticmethod
    def save_device(device_id: str, device_name: str, location: str = None, 
                   description: str = None, status: str = 'active', 
                   firmware_version: str = None, last_seen: datetime = None) -> None:
        """Save or update device in database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        if last_seen is None:
            last_seen = datetime.now()
        
        cursor.execute('''
            INSERT INTO devices (device_id, device_name, location, description, status, firmware_version, last_seen)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(device_id) DO UPDATE SET
                device_name = excluded.device_name,
                location = excluded.location,
                description = excluded.description,
                status = excluded.status,
                firmware_version = excluded.firmware_version,
                last_seen = excluded.last_seen
        ''', (device_id, device_name, location, description, status, firmware_version, last_seen))
        
        conn.commit()
        logger.debug(f"[Database] Saved device: {device_id}")
    
    @staticmethod
    def get_all_devices() -> List[Dict]:
        """Get all devices from database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('SELECT * FROM devices ORDER BY last_seen DESC')
        rows = cursor.fetchall()
        
        return [{
            'device_id': row['device_id'],
            'device_name': row['device_name'],
            'location': row['location'],
            'description': row['description'],
            'status': row['status'],
            'firmware_version': row['firmware_version'],
            'registered_at': row['registered_at'],
            'last_seen': row['last_seen']
        } for row in rows]
    
    @staticmethod
    def get_device(device_id: str) -> Optional[Dict]:
        """Get single device from database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('SELECT * FROM devices WHERE device_id = ?', (device_id,))
        row = cursor.fetchone()
        
        if row:
            return {
                'device_id': row['device_id'],
                'device_name': row['device_name'],
                'location': row['location'],
                'description': row['description'],
                'status': row['status'],
                'firmware_version': row['firmware_version'],
                'registered_at': row['registered_at'],
                'last_seen': row['last_seen']
            }
        return None
    
    @staticmethod
    def update_device_last_seen(device_id: str) -> None:
        """Update device last_seen timestamp"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            UPDATE devices SET last_seen = ? WHERE device_id = ?
        ''', (datetime.now(), device_id))
        
        conn.commit()
    
    @staticmethod
    def update_device_firmware(device_id: str, firmware_version: str) -> bool:
        """Update device firmware version"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            UPDATE devices SET firmware_version = ? WHERE device_id = ?
        ''', (firmware_version, device_id))
        
        conn.commit()
        
        return cursor.rowcount > 0
    
    @staticmethod
    def delete_device(device_id: str) -> bool:
        """Delete device from database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('DELETE FROM devices WHERE device_id = ?', (device_id,))
        conn.commit()
        
        return cursor.rowcount > 0
    
    # ============================================
    # COMMAND OPERATIONS
    # ============================================
    
    @staticmethod
    def save_command(command_id: str, device_id: str, command: Dict) -> int:
        """Save new command to database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO commands (command_id, device_id, command, status)
            VALUES (?, ?, ?, 'pending')
        ''', (command_id, device_id, json.dumps(command)))
        
        conn.commit()
        return cursor.lastrowid
    
    @staticmethod
    def get_pending_commands(device_id: str) -> List[Dict]:
        """Get all pending commands for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM commands 
            WHERE device_id = ? AND status = 'pending'
            ORDER BY created_at ASC
        ''', (device_id,))
        
        rows = cursor.fetchall()
        return [{
            'id': row['id'],
            'command_id': row['command_id'],
            'device_id': row['device_id'],
            'command': json.loads(row['command']),
            'status': row['status'],
            'created_at': row['created_at']
        } for row in rows]
    
    @staticmethod
    def get_command_by_id(command_id: str) -> Optional[Dict]:
        """Get single command by ID"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('SELECT * FROM commands WHERE command_id = ?', (command_id,))
        row = cursor.fetchone()
        
        if row:
            return {
                'id': row['id'],
                'command_id': row['command_id'],
                'device_id': row['device_id'],
                'command': json.loads(row['command']),
                'status': row['status'],
                'result': json.loads(row['result']) if row['result'] else None,
                'error_msg': row['error_msg'],
                'created_at': row['created_at'],
                'acknowledged_at': row['acknowledged_at']
            }
        return None
    
    @staticmethod
    def update_command_status(command_id: str, status: str, result: Dict = None, error_msg: str = None) -> bool:
        """Update command execution status"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            UPDATE commands 
            SET status = ?, result = ?, error_msg = ?, acknowledged_at = CURRENT_TIMESTAMP
            WHERE command_id = ?
        ''', (status, json.dumps(result) if result else None, error_msg, command_id))
        
        conn.commit()
        return cursor.rowcount > 0
    
    @staticmethod
    def get_command_history(device_id: str, limit: int = 100) -> List[Dict]:
        """Get command history for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM commands 
            WHERE device_id = ? 
            ORDER BY created_at DESC 
            LIMIT ?
        ''', (device_id, limit))
        
        rows = cursor.fetchall()
        return [{
            'id': row['id'],
            'command_id': row['command_id'],
            'device_id': row['device_id'],
            'command': json.loads(row['command']),
            'status': row['status'],
            'result': json.loads(row['result']) if row['result'] else None,
            'error_msg': row['error_msg'],
            'created_at': convert_utc_to_local(row['created_at']),
            'acknowledged_at': convert_utc_to_local(row['acknowledged_at'])
        } for row in rows]
    
    # ============================================
    # CONFIGURATION OPERATIONS
    # ============================================
    
    @staticmethod
    def save_config(device_id: str, config: Dict) -> int:
        """Save new config (replaces pending if exists)"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # First, delete any existing pending config for this device
        cursor.execute('''
            DELETE FROM configurations 
            WHERE device_id = ? AND status = 'pending'
        ''', (device_id,))
        
        # Insert new pending config
        cursor.execute('''
            INSERT INTO configurations (device_id, config, status)
            VALUES (?, ?, 'pending')
        ''', (device_id, json.dumps(config)))
        
        conn.commit()
        return cursor.lastrowid
    
    @staticmethod
    def get_latest_active_config(device_id: str) -> Optional[Dict]:
        """Get the latest acknowledged/applied (active) configuration for a device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM configurations 
            WHERE device_id = ? AND status IN ('acknowledged', 'applied')
            ORDER BY acknowledged_at DESC 
            LIMIT 1
        ''', (device_id,))
        
        row = cursor.fetchone()
        if row:
            config_data = json.loads(row['config'])
            # The config is nested inside 'config_update' key, extract it
            if 'config_update' in config_data:
                return config_data['config_update']
            return config_data
        return None
    
    @staticmethod
    def get_pending_config(device_id: str) -> Optional[Dict]:
        """Get pending config for device (only ONE)"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM configurations 
            WHERE device_id = ? AND status = 'pending'
            ORDER BY created_at DESC 
            LIMIT 1
        ''', (device_id,))
        
        row = cursor.fetchone()
        if row:
            return {
                'id': row['id'],
                'device_id': row['device_id'],
                'config': json.loads(row['config']),
                'status': row['status'],
                'created_at': row['created_at']
            }
        return None
    
    @staticmethod
    def update_config_status(device_id: str, status: str, error_msg: str = None) -> bool:
        """Update config acknowledgment status"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # Update the most recent pending config
        cursor.execute('''
            UPDATE configurations 
            SET status = ?, error_msg = ?, acknowledged_at = CURRENT_TIMESTAMP
            WHERE device_id = ? AND status = 'pending'
        ''', (status, error_msg, device_id))
        
        conn.commit()
        return cursor.rowcount > 0
    
    @staticmethod
    def get_config_history(device_id: str, limit: int = 50) -> List[Dict]:
        """Get config history for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM configurations 
            WHERE device_id = ? 
            ORDER BY created_at DESC 
            LIMIT ?
        ''', (device_id, limit))
        
        rows = cursor.fetchall()
        return [{
            'id': row['id'],
            'device_id': row['device_id'],
            'config': json.loads(row['config']),
            'status': row['status'],
            'error_msg': row['error_msg'],
            'created_at': row['created_at'],
            'acknowledged_at': row['acknowledged_at']
        } for row in rows]
    
    # ============================================
    # OTA UPDATE OPERATIONS
    # ============================================
    
    @staticmethod
    def save_ota_update(device_id: str, from_version: str, to_version: str, 
                       firmware_size: int = None, chunks_total: int = None, status: str = 'initiated') -> int:
        """Save new OTA update record"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO ota_updates (device_id, from_version, to_version, firmware_size, chunks_total, status)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (device_id, from_version, to_version, firmware_size, chunks_total, status))
        
        conn.commit()
        return cursor.lastrowid
    
    @staticmethod
    def update_ota_download_status(device_id: str, status: str, chunks_downloaded: int = None, error_msg: str = None) -> bool:
        """Update OTA download status"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        update_fields = ['status = ?', 'download_status = ?']
        params = [status, status]
        
        if chunks_downloaded is not None:
            update_fields.append('chunks_downloaded = ?')
            params.append(chunks_downloaded)
        
        if error_msg:
            update_fields.append('error_msg = ?')
            params.append(error_msg)
        
        if status in ['downloaded', 'failed']:
            update_fields.append('download_completed_at = CURRENT_TIMESTAMP')
        
        params.extend([device_id, 'initiated', 'downloading'])
        
        cursor.execute(f'''
            UPDATE ota_updates 
            SET {', '.join(update_fields)}
            WHERE device_id = ? AND status IN (?, ?)
            ORDER BY initiated_at DESC
            LIMIT 1
        ''', params)
        
        conn.commit()
        return cursor.rowcount > 0
    
    @staticmethod
    def update_ota_install_status(device_id: str, to_version: str, status: str, error_msg: str = None) -> bool:
        """Update OTA installation status (called after reboot)"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # Map ESP32 status to database status
        db_status = 'completed' if status == 'success' else 'failed'
        
        # Check current status - don't overwrite 'failed' with 'completed' 
        # This prevents verification_failed status from being overwritten
        cursor.execute('''
            SELECT status FROM ota_updates 
            WHERE device_id = ? AND to_version = ?
            ORDER BY initiated_at DESC
            LIMIT 1
        ''', (device_id, to_version))
        
        row = cursor.fetchone()
        if row and row['status'] == 'failed' and db_status == 'completed':
            logger.warning(f"[Database] OTA status for {device_id} already 'failed', not overwriting with 'completed'")
            # Still update install_status and install_completed_at, but keep status as 'failed'
            cursor.execute('''
                UPDATE ota_updates 
                SET install_status = ?, install_completed_at = CURRENT_TIMESTAMP
                WHERE device_id = ? AND to_version = ?
                ORDER BY initiated_at DESC
                LIMIT 1
            ''', (status, device_id, to_version))
        else:
            cursor.execute('''
                UPDATE ota_updates 
                SET status = ?, install_status = ?, error_msg = ?, install_completed_at = CURRENT_TIMESTAMP
                WHERE device_id = ? AND to_version = ?
                ORDER BY initiated_at DESC
                LIMIT 1
            ''', (db_status, status, error_msg, device_id, to_version))
        
        conn.commit()
        return cursor.rowcount > 0
    
    @staticmethod
    def update_ota_status(device_id: str, status: str, error_msg: str = None) -> bool:
        """Update OTA status for latest update"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        if error_msg:
            cursor.execute('''
                UPDATE ota_updates 
                SET status = ?, error_msg = ?
                WHERE device_id = ?
                ORDER BY initiated_at DESC
                LIMIT 1
            ''', (status, error_msg, device_id))
        else:
            cursor.execute('''
                UPDATE ota_updates 
                SET status = ?
                WHERE device_id = ?
                ORDER BY initiated_at DESC
                LIMIT 1
            ''', (status, device_id))
        
        conn.commit()
        return cursor.rowcount > 0
    
    @staticmethod
    def get_latest_ota_update(device_id: str) -> Optional[Dict]:
        """Get latest OTA update record for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM ota_updates 
            WHERE device_id = ? 
            ORDER BY initiated_at DESC 
            LIMIT 1
        ''', (device_id,))
        
        row = cursor.fetchone()
        if row:
            return {
                'id': row['id'],
                'device_id': row['device_id'],
                'from_version': row['from_version'],
                'to_version': row['to_version'],
                'status': row['status'],
                'download_status': row['download_status'],
                'install_status': row['install_status'],
                'error_msg': row['error_msg'],
                'firmware_size': row['firmware_size'],
                'chunks_total': row['chunks_total'],
                'chunks_downloaded': row['chunks_downloaded'],
                'initiated_at': row['initiated_at'],
                'download_completed_at': row['download_completed_at'],
                'install_completed_at': row['install_completed_at']
            }
        return None
    
    @staticmethod
    def get_ota_history(device_id: str, limit: int = 20) -> List[Dict]:
        """Get OTA update history for device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM ota_updates 
            WHERE device_id = ? 
            ORDER BY initiated_at DESC 
            LIMIT ?
        ''', (device_id, limit))
        
        rows = cursor.fetchall()
        return [{
            'id': row['id'],
            'device_id': row['device_id'],
            'from_version': row['from_version'],
            'to_version': row['to_version'],
            'status': row['status'],
            'download_status': row['download_status'],
            'install_status': row['install_status'],
            'error_msg': row['error_msg'],
            'firmware_size': row['firmware_size'],
            'chunks_total': row['chunks_total'],
            'chunks_downloaded': row['chunks_downloaded'],
            'initiated_at': convert_utc_to_local(row['initiated_at']),
            'download_completed_at': convert_utc_to_local(row['download_completed_at']),
            'install_completed_at': convert_utc_to_local(row['install_completed_at'])
        } for row in rows]
    
    @staticmethod
    def get_ota_statistics() -> Dict:
        """Get OTA statistics from database"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # Total updates initiated
        cursor.execute('SELECT COUNT(*) as count FROM ota_updates')
        total_updates = cursor.fetchone()['count']
        
        # Successful updates (completed status)
        cursor.execute("SELECT COUNT(*) as count FROM ota_updates WHERE status = 'completed'")
        successful_updates = cursor.fetchone()['count']
        
        # Failed updates (failed or error status)
        cursor.execute("SELECT COUNT(*) as count FROM ota_updates WHERE status IN ('failed', 'error')")
        failed_updates = cursor.fetchone()['count']
        
        # Active sessions (initiated, downloading, installing statuses)
        cursor.execute("SELECT COUNT(*) as count FROM ota_updates WHERE status IN ('initiated', 'downloading', 'installing')")
        active_sessions = cursor.fetchone()['count']
        
        # Success rate
        success_rate = 0.0
        if total_updates > 0:
            success_rate = round((successful_updates / total_updates) * 100, 2)
        
        # Log for debugging
        logger.debug(f"OTA Statistics: total={total_updates}, successful={successful_updates}, failed={failed_updates}, active={active_sessions}")
        
        return {
            'total_updates_initiated': total_updates,
            'successful_updates': successful_updates,
            'failed_updates': failed_updates,
            'active_sessions': active_sessions,
            'success_rate': success_rate
        }
    
    # ============================================
    # POWER MANAGEMENT OPERATIONS
    # ============================================
    
    @staticmethod
    def save_power_report(device_id: str, timestamp: int, enabled: bool, techniques: int,
                         avg_current_ma: float, energy_saved_mah: float, uptime_ms: int,
                         high_perf_ms: int = 0, normal_ms: int = 0, 
                         low_power_ms: int = 0, sleep_ms: int = 0) -> int:
        """
        Save power report from ESP32 to database
        
        Args:
            device_id: Device identifier
            timestamp: Report timestamp in milliseconds (from ESP32)
            enabled: Power management enabled status
            techniques: Bitmask of active techniques (0x00-0x0F)
            avg_current_ma: Average current consumption in milliamps
            energy_saved_mah: Energy saved in milliamp-hours
            uptime_ms: Device uptime in milliseconds
            high_perf_ms: Time spent in high performance mode
            normal_ms: Time spent in normal mode
            low_power_ms: Time spent in low power mode
            sleep_ms: Time spent in sleep mode
            
        Returns:
            Row ID of inserted report
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # Convert millisecond timestamp to UTC datetime
        report_datetime = datetime.fromtimestamp(timestamp / 1000.0, tz=timezone.utc)
        
        cursor.execute('''
            INSERT INTO power_reports 
            (device_id, timestamp, enabled, techniques, avg_current_ma, energy_saved_mah, 
             uptime_ms, high_perf_ms, normal_ms, low_power_ms, sleep_ms)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (device_id, report_datetime, int(enabled), techniques, avg_current_ma, 
              energy_saved_mah, uptime_ms, high_perf_ms, normal_ms, low_power_ms, sleep_ms))
        
        conn.commit()
        return cursor.lastrowid
    
    @staticmethod
    def get_power_reports(device_id: str, period: str = '24h', limit: int = 100) -> List[Dict]:
        """
        Get power reports for a device within a time period
        
        Args:
            device_id: Device identifier
            period: Time period ('1h', '24h', '7d', '30d', 'all')
            limit: Maximum number of records to return
            
        Returns:
            List of power report dictionaries
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        # Handle 'all' period - no time filter
        if period == 'all':
            cursor.execute('''
                SELECT device_id, timestamp, enabled, techniques, avg_current_ma, energy_saved_mah,
                       uptime_ms, high_perf_ms, normal_ms, low_power_ms, sleep_ms, created_at
                FROM power_reports
                WHERE device_id = ?
                ORDER BY timestamp DESC
                LIMIT ?
            ''', (device_id, limit))
        else:
            # Calculate cutoff datetime based on period
            period_map = {
                '1h': timedelta(hours=1),
                '24h': timedelta(hours=24),
                '7d': timedelta(days=7),
                '30d': timedelta(days=30)
            }
            
            if period not in period_map:
                period = '24h'  # Default to 24 hours
            
            cutoff_datetime = datetime.now() - period_map[period]
            
            cursor.execute('''
                SELECT device_id, timestamp, enabled, techniques, avg_current_ma, energy_saved_mah,
                       uptime_ms, high_perf_ms, normal_ms, low_power_ms, sleep_ms, created_at
                FROM power_reports
                WHERE device_id = ? AND timestamp >= ?
                ORDER BY timestamp DESC
                LIMIT ?
            ''', (device_id, cutoff_datetime, limit))
        
        reports = []
        for row in cursor.fetchall():
            reports.append({
                'device_id': row['device_id'],
                'timestamp': row['timestamp'],
                'enabled': bool(row['enabled']),
                'techniques': row['techniques'],
                'avg_current_ma': row['avg_current_ma'],
                'energy_saved_mah': row['energy_saved_mah'],
                'uptime_ms': row['uptime_ms'],
                'high_perf_ms': row['high_perf_ms'],
                'normal_ms': row['normal_ms'],
                'low_power_ms': row['low_power_ms'],
                'sleep_ms': row['sleep_ms'],
                'created_at': row['created_at']
            })
        
        return reports
    
    @staticmethod
    def get_latest_power_report(device_id: str) -> Optional[Dict]:
        """Get the most recent power report for a device"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT device_id, timestamp, enabled, techniques, avg_current_ma, energy_saved_mah,
                   uptime_ms, high_perf_ms, normal_ms, low_power_ms, sleep_ms, created_at
            FROM power_reports
            WHERE device_id = ?
            ORDER BY timestamp DESC
            LIMIT 1
        ''', (device_id,))
        
        row = cursor.fetchone()
        if not row:
            return None
        
        return {
            'device_id': row['device_id'],
            'timestamp': row['timestamp'],
            'enabled': bool(row['enabled']),
            'techniques': row['techniques'],
            'avg_current_ma': row['avg_current_ma'],
            'energy_saved_mah': row['energy_saved_mah'],
            'uptime_ms': row['uptime_ms'],
            'high_perf_ms': row['high_perf_ms'],
            'normal_ms': row['normal_ms'],
            'low_power_ms': row['low_power_ms'],
            'sleep_ms': row['sleep_ms'],
            'created_at': row['created_at']
        }
    
    # ============================================
    # CLEANUP OPERATIONS
    # ============================================
    
    @staticmethod
    def cleanup_old_data():
        """
        Delete data older than RETENTION_DAYS
        If RETENTION_DAYS is None, this function does nothing (unlimited retention)
        Can be called manually via API endpoint if needed
        """
        if RETENTION_DAYS is None:
            logger.info("Cleanup skipped - unlimited retention is enabled")
            return {
                'sensor_data': 0,
                'commands': 0,
                'configurations': 0,
                'ota_updates': 0,
                'power_reports': 0,
                'message': 'Cleanup skipped - unlimited retention enabled'
            }
        
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cutoff_date = datetime.now() - timedelta(days=RETENTION_DAYS)
        
        # Clean sensor data
        cursor.execute('DELETE FROM sensor_data WHERE created_at < ?', (cutoff_date,))
        sensor_deleted = cursor.rowcount
        
        # Clean old commands (keep all if status != 'pending', but delete old pending)
        cursor.execute('DELETE FROM commands WHERE created_at < ? AND status = ?', (cutoff_date, 'pending'))
        commands_deleted = cursor.rowcount
        
        # Clean old configs (keep recent applied/failed for history)
        cursor.execute('DELETE FROM configurations WHERE created_at < ? AND status != ?', (cutoff_date, 'pending'))
        config_deleted = cursor.rowcount
        
        # Clean old OTA updates (keep recent for history)
        cursor.execute('DELETE FROM ota_updates WHERE initiated_at < ?', (cutoff_date,))
        ota_deleted = cursor.rowcount
        
        # Clean old power reports (keep last 7 days detailed, aggregate older data)
        # For now, keep 7 days of detailed reports
        power_cutoff = datetime.now() - timedelta(days=7)
        cursor.execute('DELETE FROM power_reports WHERE created_at < ?', (power_cutoff,))
        power_deleted = cursor.rowcount
        
        conn.commit()
        
        if sensor_deleted + commands_deleted + config_deleted + ota_deleted + power_deleted > 0:
            logger.info(f"[Database] Cleanup: Deleted {sensor_deleted} sensor records, "
                  f"{commands_deleted} commands, {config_deleted} configs, {ota_deleted} OTA records, "
                  f"{power_deleted} power reports")
        
        return {
            'sensor_data': sensor_deleted,
            'commands': commands_deleted,
            'configurations': config_deleted,
            'ota_updates': ota_deleted,
            'power_reports': power_deleted
        }
    
    @staticmethod
    def get_database_stats() -> Dict:
        """Get database statistics"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        stats = {}
        
        # Count sensor data
        cursor.execute('SELECT COUNT(*) as count FROM sensor_data')
        stats['sensor_data_count'] = cursor.fetchone()['count']
        
        # Count commands
        cursor.execute('SELECT status, COUNT(*) as count FROM commands GROUP BY status')
        stats['commands'] = {row['status']: row['count'] for row in cursor.fetchall()}
        
        # Count configs
        cursor.execute('SELECT status, COUNT(*) as count FROM configurations GROUP BY status')
        stats['configurations'] = {row['status']: row['count'] for row in cursor.fetchall()}
        
        # Count OTA updates
        cursor.execute('SELECT status, COUNT(*) as count FROM ota_updates GROUP BY status')
        stats['ota_updates'] = {row['status']: row['count'] for row in cursor.fetchall()}
        
        # Database file size
        if DB_PATH.exists():
            stats['database_size_mb'] = DB_PATH.stat().st_size / (1024 * 1024)
        
        return stats

    # ============================================================
    # Fault Injection & Recovery (Milestone 5)
    # ============================================================
    
    @staticmethod
    def save_fault_injection(device_id: Optional[str], fault_type: str, backend: str, 
                            error_type: Optional[str] = None, exception_code: Optional[int] = None,
                            delay_ms: Optional[int] = None, success: bool = True, 
                            error_msg: Optional[str] = None) -> int:
        """
        Save fault injection event to database
        
        Args:
            device_id: Target device ID (None for all devices)
            fault_type: Type of fault injected
            backend: Backend used (inverter_sim, local)
            error_type: Inverter SIM error type (CRC_ERROR, CORRUPT, etc.)
            exception_code: Modbus exception code (if applicable)
            delay_ms: Delay in milliseconds (if applicable)
            success: Whether injection was successful
            error_msg: Error message if failed
            
        Returns:
            ID of inserted record
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO fault_injections 
            (device_id, fault_type, backend, error_type, exception_code, delay_ms, success, error_msg)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ''', (device_id, fault_type, backend, error_type, exception_code, delay_ms, 
              1 if success else 0, error_msg))
        
        conn.commit()
        logger.info(f"[Database] Saved fault injection: {fault_type} ({backend})")
        return cursor.lastrowid
    
    @staticmethod
    def get_fault_injection_history(device_id: Optional[str] = None, limit: int = 100) -> List[Dict]:
        """
        Get fault injection history
        
        Args:
            device_id: Filter by device ID (None for all)
            limit: Maximum number of records to return
            
        Returns:
            List of fault injection records
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        if device_id:
            cursor.execute('''
                SELECT * FROM fault_injections 
                WHERE device_id = ? OR device_id IS NULL
                ORDER BY created_at DESC 
                LIMIT ?
            ''', (device_id, limit))
        else:
            cursor.execute('''
                SELECT * FROM fault_injections 
                ORDER BY created_at DESC 
                LIMIT ?
            ''', (limit,))
        
        rows = cursor.fetchall()
        # Convert timestamps to local time for proper display
        result = []
        for row in rows:
            row_dict = dict(row)
            if row_dict.get('created_at'):
                row_dict['created_at'] = convert_utc_to_local(row_dict['created_at'])
            result.append(row_dict)
        return result
    
    @staticmethod
    def save_recovery_event(device_id: str, timestamp: int, fault_type: str, 
                           recovery_action: str, success: bool, details: str = '',
                           retry_count: int = 0) -> int:
        """
        Save fault recovery event from ESP32
        
        Args:
            device_id: Device that experienced the fault
            timestamp: ESP32 timestamp when fault occurred
            fault_type: Type of fault detected
            recovery_action: Action taken to recover
            success: Whether recovery was successful
            details: Additional details
            retry_count: Number of retry attempts
            
        Returns:
            ID of inserted record
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO fault_recovery_events 
            (device_id, timestamp, fault_type, recovery_action, success, details, retry_count)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        ''', (device_id, timestamp, fault_type, recovery_action, 
              1 if success else 0, details, retry_count))
        
        conn.commit()
        logger.info(f"[Database] Saved recovery event: {device_id} - {fault_type} - Success: {success}")
        return cursor.lastrowid
    
    @staticmethod
    def get_recovery_events(device_id: Optional[str] = None, limit: int = 100) -> List[Dict]:
        """
        Get fault recovery event history
        
        Args:
            device_id: Filter by device ID (None for all)
            limit: Maximum number of records to return
            
        Returns:
            List of recovery event records
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        if device_id:
            cursor.execute('''
                SELECT * FROM fault_recovery_events 
                WHERE device_id = ?
                ORDER BY received_at DESC 
                LIMIT ?
            ''', (device_id, limit))
        else:
            cursor.execute('''
                SELECT * FROM fault_recovery_events 
                ORDER BY received_at DESC 
                LIMIT ?
            ''', (limit,))
        
        rows = cursor.fetchall()
        # Convert received_at timestamps to local time for proper display
        result = []
        for row in rows:
            row_dict = dict(row)
            if row_dict.get('received_at'):
                row_dict['received_at'] = convert_utc_to_local(row_dict['received_at'])
            result.append(row_dict)
        return result
    
    @staticmethod
    def get_recovery_statistics(device_id: Optional[str] = None) -> Dict:
        """
        Get recovery statistics
        
        Args:
            device_id: Filter by device ID (None for all)
            
        Returns:
            Dictionary with statistics
        """
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        if device_id:
            cursor.execute('''
                SELECT 
                    COUNT(*) as total,
                    SUM(success) as successful,
                    COUNT(*) - SUM(success) as failed
                FROM fault_recovery_events
                WHERE device_id = ?
            ''', (device_id,))
        else:
            cursor.execute('''
                SELECT 
                    COUNT(*) as total,
                    SUM(success) as successful,
                    COUNT(*) - SUM(success) as failed
                FROM fault_recovery_events
            ''')
        
        row = cursor.fetchone()
        return {
            'total': row['total'] or 0,
            'successful': row['successful'] or 0,
            'failed': row['failed'] or 0,
            'success_rate': (row['successful'] / row['total'] * 100) if row['total'] > 0 else 0
        }


# Initialize database on module import
Database.init_database()
