"""
Database handler for EcoWatt Flask Server
Provides persistent storage for sensor data, commands, configs, and OTA updates
Uses SQLite with 7-day auto-cleanup
"""

import sqlite3
import json
from datetime import datetime, timedelta
from pathlib import Path
import threading
from typing import Dict, List, Optional, Any

# Thread-safe database connection pool
_thread_local = threading.local()

# Database configuration
DB_PATH = Path(__file__).parent / 'ecowatt_data.db'
RETENTION_DAYS = 7

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
        
        conn.commit()
        print("[Database] Schema initialized successfully")
        
        # Run initial cleanup
        Database.cleanup_old_data()
    
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
            'created_at': row['created_at'],
            'acknowledged_at': row['acknowledged_at']
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
                       firmware_size: int = None, chunks_total: int = None) -> int:
        """Save new OTA update record"""
        conn = Database.get_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO ota_updates (device_id, from_version, to_version, firmware_size, chunks_total, status)
            VALUES (?, ?, ?, ?, ?, 'initiated')
        ''', (device_id, from_version, to_version, firmware_size, chunks_total))
        
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
        
        cursor.execute('''
            UPDATE ota_updates 
            SET status = ?, install_status = ?, error_msg = ?, install_completed_at = CURRENT_TIMESTAMP
            WHERE device_id = ? AND to_version = ?
            ORDER BY initiated_at DESC
            LIMIT 1
        ''', (status, status, error_msg, device_id, to_version))
        
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
            'initiated_at': row['initiated_at'],
            'download_completed_at': row['download_completed_at'],
            'install_completed_at': row['install_completed_at']
        } for row in rows]
    
    # ============================================
    # CLEANUP OPERATIONS
    # ============================================
    
    @staticmethod
    def cleanup_old_data():
        """Delete data older than RETENTION_DAYS (7 days)"""
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
        
        conn.commit()
        
        if sensor_deleted + commands_deleted + config_deleted + ota_deleted > 0:
            print(f"[Database] Cleanup: Deleted {sensor_deleted} sensor records, "
                  f"{commands_deleted} commands, {config_deleted} configs, {ota_deleted} OTA records")
        
        return {
            'sensor_data': sensor_deleted,
            'commands': commands_deleted,
            'configurations': config_deleted,
            'ota_updates': ota_deleted
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


# Initialize database on module import
Database.init_database()
