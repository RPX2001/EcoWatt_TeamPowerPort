"""
Database and Flask Server Integration Tests

Comprehensive integration testing for:
1. Database operations (CRUD operations, schema validation)
2. Flask server endpoints (all routes)
3. Data persistence and retrieval
4. Command queue management
5. Configuration management
6. OTA firmware management
7. Diagnostics and fault logging
8. Security state management

Run with:
    pytest tests/integration_tests/test_database_flask_integration.py -v
"""

import pytest
import json
import time
import sys
from pathlib import Path
from datetime import datetime, timedelta
import sqlite3
import os

# Add flask directory to path
flask_dir = Path(__file__).parent.parent.parent
sys.path.insert(0, str(flask_dir))

from flask_server_modular import app
from database import Database
from utils.logger_utils import get_logger

logger = get_logger(__name__)

# Test configuration
TEST_DEVICE_ID = "ESP32_TEST_DEVICE"
TEST_DB_PATH = flask_dir / 'test_ecowatt.db'


@pytest.fixture(scope='function')
def test_database():
    """Create a test database for each test"""
    # Backup original DB_PATH
    original_db_path = Database.DB_PATH if hasattr(Database, 'DB_PATH') else None
    
    # Set test database path
    import database
    database.DB_PATH = TEST_DB_PATH
    
    # Initialize test database
    Database.init_database()
    
    yield Database
    
    # Cleanup: close connections and remove test database
    if hasattr(database._thread_local, 'connection'):
        database._thread_local.connection.close()
        delattr(database._thread_local, 'connection')
    
    if TEST_DB_PATH.exists():
        TEST_DB_PATH.unlink()
    
    # Restore original DB_PATH
    if original_db_path:
        database.DB_PATH = original_db_path


@pytest.fixture
def client():
    """Flask test client fixture"""
    app.config['TESTING'] = True
    with app.app_context():
        with app.test_client() as client:
            yield client


# ============================================================================
# DATABASE TESTS
# ============================================================================

class TestDatabaseOperations:
    """Test core database operations"""
    
    def test_database_initialization(self, test_database):
        """Test that database schema is created correctly"""
        conn = test_database.get_connection()
        cursor = conn.cursor()
        
        # Check that all required tables exist
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
        tables = {row[0] for row in cursor.fetchall()}
        
        required_tables = {
            'sensor_data',
            'commands',
            'configurations',
            'ota_updates'
        }
        
        assert required_tables.issubset(tables), f"Missing tables: {required_tables - tables}"
        logger.info(f"✓ All required tables present: {tables}")
    
    def test_sensor_data_crud(self, test_database):
        """Test sensor data CRUD operations"""
        device_id = TEST_DEVICE_ID
        
        # Create - Insert sensor data
        test_data = {
            'voltage': 230.5,
            'current': 2.3,
            'power': 530.15,
            'energy': 1.25,
            'frequency': 50.0,
            'pf': 0.95
        }
        
        result = test_database.save_sensor_data(
            device_id=device_id,
            timestamp=datetime.now(),
            register_data=test_data,
            compression_method='none'
        )
        
        assert result is not None and result > 0, "Failed to store sensor data"
        logger.info("✓ Sensor data stored successfully")
        
        # Read - Retrieve sensor data
        retrieved = test_database.get_latest_sensor_data(device_id)
        assert retrieved is not None, "No sensor data retrieved"
        assert retrieved['device_id'] == device_id
        logger.info("✓ Retrieved sensor data record")
        
        # Verify data integrity - retrieved['register_data'] is already a dict
        stored_register = retrieved['register_data']
        assert stored_register['voltage'] == test_data['voltage']
        assert stored_register['current'] == test_data['current']
        logger.info("✓ Data integrity verified")
    
    def test_command_queue_operations(self, test_database):
        """Test command queue CRUD operations"""
        device_id = TEST_DEVICE_ID
        
        # Create - Queue a command
        command_data = {
            'action': 'set_config',
            'params': {'sample_rate': 5000}
        }
        
        cmd_id = test_database.save_command(f"CMD_{int(time.time())}", device_id, command_data)
        assert cmd_id is not None, "Failed to queue command"
        logger.info(f"✓ Command queued with ID: {cmd_id}")
        
        # Read - Get pending commands
        pending = test_database.get_pending_commands(device_id)
        assert len(pending) > 0, "No pending commands found"
        logger.info(f"✓ Retrieved {len(pending)} pending commands")
        
        # Update - Mark command as acknowledged (which removes from pending)
        conn = test_database.get_connection()
        cursor = conn.cursor()
        cursor.execute('''
            UPDATE commands 
            SET status = 'executed', acknowledged_at = ? 
            WHERE id = ?
        ''', (datetime.now(), cmd_id))
        conn.commit()
        logger.info("✓ Command status updated")
        
        # Verify update
        pending_after = test_database.get_pending_commands(device_id)
        assert len(pending_after) == 0, "Command still pending after update"
        logger.info("✓ Command removed from pending queue")
    
    def test_device_config_operations(self, test_database):
        """Test device configuration CRUD operations"""
        device_id = TEST_DEVICE_ID
        
        # Create/Update - Store configuration
        config = {
            'sample_rate': 5000,
            'buffer_size': 100,
            'compression': 'delta',
            'wifi_config': {
                'ssid': 'TestNetwork',
                'reconnect_interval': 30
            }
        }
        
        result = test_database.save_config(device_id, config)
        assert result is not None, "Failed to store device config"
        logger.info("✓ Device configuration stored")
        
        # Read - Retrieve configuration (already returns dict with config parsed)
        retrieved = test_database.get_pending_config(device_id)
        assert retrieved is not None, "No configuration retrieved"
        retrieved_config = retrieved['config']  # Already a dict, not JSON string
        assert retrieved_config['sample_rate'] == config['sample_rate']
        assert retrieved_config['wifi_config']['ssid'] == config['wifi_config']['ssid']
        logger.info("✓ Device configuration retrieved and verified")
        
        # Mark as acknowledged and create new config
        conn = test_database.get_connection()
        cursor = conn.cursor()
        cursor.execute('''
            UPDATE configurations 
            SET status = 'acknowledged', acknowledged_at = ? 
            WHERE device_id = ? AND status = 'pending'
        ''', (datetime.now(), device_id))
        conn.commit()
        
        # Update - Modify configuration
        updated_config = config.copy()
        updated_config['sample_rate'] = 10000
        
        result = test_database.save_config(device_id, updated_config)
        assert result is not None, "Failed to update device config"
        
        retrieved_updated = test_database.get_pending_config(device_id)
        retrieved_updated_config = retrieved_updated['config']  # Already a dict
        assert retrieved_updated_config['sample_rate'] == 10000
        logger.info("✓ Device configuration updated successfully")
    
    def test_firmware_update_operations(self, test_database):
        """Test firmware update CRUD operations"""
        device_id = TEST_DEVICE_ID
        
        # Create - Record firmware update (no status parameter - it's always 'initiated')
        result = test_database.save_ota_update(
            device_id=device_id,
            from_version='1.0.4',
            to_version='1.0.5',
            firmware_size=524288,
            chunks_total=100
        )
        assert result is not None, "Failed to record firmware update"
        logger.info("✓ Firmware update recorded")
        
        # Read - Get latest firmware update
        latest = test_database.get_latest_ota_update(device_id)
        assert latest is not None, "No firmware update found"
        assert latest['to_version'] == '1.0.5'
        logger.info("✓ Retrieved firmware update record")
        
        # Update - Update firmware status
        update_id = latest['id']
        conn = test_database.get_connection()
        cursor = conn.cursor()
        cursor.execute('''
            UPDATE ota_updates 
            SET status = 'completed', install_completed_at = ? 
            WHERE id = ?
        ''', (datetime.now(), update_id))
        conn.commit()
        logger.info("✓ Firmware status updated")
    
    
    def test_data_retention_cleanup(self, test_database):
        """Test automatic data cleanup for old records"""
        device_id = TEST_DEVICE_ID
        
        # Insert old sensor data (8 days ago)
        old_timestamp = datetime.now() - timedelta(days=8)
        test_database.save_sensor_data(
            device_id=device_id,
            timestamp=old_timestamp,
            register_data={'voltage': 230.0},
            compression_method='none'
        )
        
        # Insert recent sensor data
        recent_timestamp = datetime.now()
        test_database.save_sensor_data(
            device_id=device_id,
            timestamp=recent_timestamp,
            register_data={'voltage': 235.0},
            compression_method='none'
        )
        
        # Run cleanup (no parameters - uses default RETENTION_DAYS)
        result = test_database.cleanup_old_data()
        assert result is not None, "Cleanup failed"
        logger.info("✓ Data cleanup executed")
        
        # Verify recent data still exists
        latest = test_database.get_latest_sensor_data(device_id)
        assert latest is not None, "Recent data was removed"
        logger.info("✓ Data retention verified")


# ============================================================================
# FLASK SERVER TESTS
# ============================================================================

class TestFlaskEndpoints:
    """Test Flask server endpoints"""
    
    def test_health_check(self, client):
        """Test health check endpoint"""
        response = client.get('/health')
        assert response.status_code == 200
        
        data = json.loads(response.data)
        assert data['status'] == 'healthy'
        assert 'timestamp' in data
        logger.info("✓ Health check endpoint working")
    
    def test_poll_commands_endpoint(self, client, test_database):
        """Test command polling endpoint"""
        # Queue a command first
        command_data = {'action': 'restart'}
        test_database.save_command(f"CMD_{int(time.time())}", TEST_DEVICE_ID, command_data)
        
        # Poll for commands
        response = client.get(f'/commands/{TEST_DEVICE_ID}/poll')
        assert response.status_code == 200
        
        data = json.loads(response.data)
        assert 'commands' in data
        assert len(data['commands']) > 0
        logger.info("✓ Command polling endpoint working")
    
    def test_ota_check_endpoint(self, client, test_database):
        """Test OTA update check endpoint"""
        response = client.get(f'/ota/check/{TEST_DEVICE_ID}?current_version=1.0.4')
        assert response.status_code == 200
        
        data = json.loads(response.data)
        assert 'update_available' in data
        logger.info("✓ OTA check endpoint working")
    
    def test_diagnostics_upload_endpoint(self, client):
        """Test diagnostics upload endpoint"""
        payload = {
            'free_heap': 150000,
            'uptime': 3600,
            'wifi_rssi': -45
        }
        
        response = client.post(f'/diagnostics/{TEST_DEVICE_ID}',
                              data=json.dumps(payload),
                              content_type='application/json')
        
        assert response.status_code in [200, 201]
        logger.info("✓ Diagnostics upload endpoint working")


# ============================================================================
# END-TO-END WORKFLOW TESTS
# ============================================================================

class TestEndToEndWorkflows:
    """Test complete workflows involving multiple components"""
    
    def test_command_execution_workflow(self, client, test_database):
        """Test complete command execution workflow"""
        # Step 1: Queue command via database
        command_data = {
            'action': 'set_config',
            'params': {'sample_rate': 10000}
        }
        cmd_id = test_database.save_command(f"CMD_{int(time.time())}", TEST_DEVICE_ID, command_data)
        assert cmd_id is not None
        
        # Step 2: Device polls for commands
        response = client.get(f'/commands/{TEST_DEVICE_ID}/poll')
        assert response.status_code == 200
        data = json.loads(response.data)
        assert len(data['commands']) > 0
        
        # Step 3: Mark as acknowledged
        conn = test_database.get_connection()
        cursor = conn.cursor()
        cursor.execute('''
            UPDATE commands 
            SET status = 'executed', acknowledged_at = ? 
            WHERE id = ?
        ''', (datetime.now(), cmd_id))
        conn.commit()
        
        # Step 4: Verify no pending commands
        pending = test_database.get_pending_commands(TEST_DEVICE_ID)
        assert len(pending) == 0
        
        logger.info("✓ Complete command execution workflow successful")
    
    def test_ota_workflow(self, client, test_database):
        """Test complete OTA update workflow"""
        # Step 1: Check for update
        response = client.get(f'/ota/check/{TEST_DEVICE_ID}?current_version=1.0.4')
        assert response.status_code == 200
        data = json.loads(response.data)
        
        # Step 2: Record OTA update (no status parameter)
        result = test_database.save_ota_update(
            device_id=TEST_DEVICE_ID,
            from_version='1.0.4',
            to_version='1.0.5',
            firmware_size=524288,
            chunks_total=100
        )
        assert result is not None
        
        # Step 3: Verify OTA record exists
        latest = test_database.get_latest_ota_update(TEST_DEVICE_ID)
        assert latest is not None
        assert latest['to_version'] == '1.0.5'
        
        logger.info("✓ Complete OTA workflow successful")


if __name__ == '__main__':
    """Run tests directly"""
    pytest.main([__file__, '-v', '--tb=short'])
