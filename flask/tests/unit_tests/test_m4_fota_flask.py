"""
M4 FOTA - Flask Unit Tests

Tests:
1. Check for firmware update
2. Initiate OTA session
3. Get firmware chunk
4. Complete OTA session successfully
5. Complete OTA session with failure
6. Get OTA status
7. Get OTA statistics
8. Cancel OTA session
9. Multiple device sessions
10. Invalid firmware version
11. Invalid chunk index
12. Session timeout handling
13. Manifest validation
14. Chunk delivery sequence
15. Concurrent session prevention
"""

import pytest
import json
import os
import tempfile
import shutil
from pathlib import Path

try:
    from flask_server_modular import app
except ImportError:
    from sys import path
    path.insert(0, '../..')
    from flask_server_modular import app

from handlers.ota_handler import (
    check_for_update,
    initiate_ota_session,
    get_firmware_chunk_for_device,
    complete_ota_session,
    get_ota_status,
    get_ota_stats,
    cancel_ota_session,
    ota_sessions,
    ota_stats,
    FIRMWARE_DIR
)
from utils.logger_utils import get_logger

logger = get_logger(__name__)

# Test configuration
TEST_DEVICE_ID = "TEST_FOTA_DEVICE"
TEST_FIRMWARE_VERSION = "1.0.4"


# Client fixture now comes from conftest.py


@pytest.fixture(autouse=True)
def reset_ota():
    """Clear OTA sessions and stats before each test"""
    ota_sessions.clear()
    ota_stats['total_updates_initiated'] = 0
    ota_stats['successful_updates'] = 0
    ota_stats['failed_updates'] = 0
    ota_stats['active_sessions'] = 0
    ota_stats['total_bytes_transferred'] = 0
    yield


@pytest.fixture
def temp_firmware_dir():
    """Create temporary firmware directory with test files"""
    # Create temp directory
    temp_dir = tempfile.mkdtemp()
    
    # Create test manifest
    manifest = {
        'version': '1.0.4',
        'size': 1024,
        'chunk_size': 256,
        'total_chunks': 4,
        'signature': 'test_signature_123',
        'sha256_hash': 'abcd1234',
        'release_notes': 'Test firmware'
    }
    
    manifest_file = Path(temp_dir) / 'firmware_1.0.4_manifest.json'
    with open(manifest_file, 'w') as f:
        json.dump(manifest, f)
    
    # Create test firmware file (1024 bytes)
    firmware_file = Path(temp_dir) / 'firmware_1.0.4_encrypted.bin'
    firmware_data = b'X' * 1024  # 1KB test firmware
    with open(firmware_file, 'wb') as f:
        f.write(firmware_data)
    
    yield temp_dir
    
    # Cleanup
    shutil.rmtree(temp_dir, ignore_errors=True)


class TestFOTAFlask:
    """M4 FOTA Flask Test Suite"""
    
    def test_check_for_update_available(self, client, temp_firmware_dir, monkeypatch):
        """Test 1: Check for firmware update - update available"""
        logger.info("=== Test 1: Check for Update Available ===")
        
        # Monkeypatch FIRMWARE_DIR
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        update_available, update_info = check_for_update(TEST_DEVICE_ID, "1.0.0")
        
        assert update_available is True, "Update should be available"
        assert update_info is not None
        assert update_info['available'] is True
        assert update_info['latest_version'] == '1.0.4'
        assert update_info['current_version'] == '1.0.0'
        assert update_info['total_chunks'] == 4
        
        logger.info("[PASS] Update check returned correct information")
    
    def test_check_for_update_not_available(self, client, temp_firmware_dir, monkeypatch):
        """Test 2: Check for firmware update - no update needed"""
        logger.info("=== Test 2: Check for Update Not Available ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        update_available, update_info = check_for_update(TEST_DEVICE_ID, "1.0.4")
        
        assert update_available is False, "No update should be available"
        assert update_info['available'] is False
        
        logger.info("[PASS] No update when versions match")
    
    def test_initiate_ota_session(self, client, temp_firmware_dir, monkeypatch):
        """Test 3: Initiate OTA session successfully"""
        logger.info("=== Test 3: Initiate OTA Session ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        success, session_id = initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        assert success is True, "Session initiation should succeed"
        assert session_id is not None
        assert TEST_DEVICE_ID in session_id
        
        # Verify session was created
        assert TEST_DEVICE_ID in ota_sessions
        session = ota_sessions[TEST_DEVICE_ID]
        assert session['status'] == 'in_progress'
        assert session['firmware_version'] == TEST_FIRMWARE_VERSION
        assert session['total_chunks'] == 4
        
        # Verify stats updated
        assert ota_stats['total_updates_initiated'] == 1
        assert ota_stats['active_sessions'] == 1
        
        logger.info("[PASS] OTA session initiated successfully")
    
    def test_initiate_duplicate_session(self, client, temp_firmware_dir, monkeypatch):
        """Test 4: Prevent duplicate OTA sessions"""
        logger.info("=== Test 4: Prevent Duplicate Sessions ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        # First session
        success1, session_id1 = initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        assert success1 is True
        
        # Try duplicate
        success2, error = initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        assert success2 is False
        assert 'already in progress' in error.lower()
        
        logger.info("[PASS] Duplicate session prevented")
    
    def test_get_firmware_chunk(self, client, temp_firmware_dir, monkeypatch):
        """Test 5: Get firmware chunk successfully"""
        logger.info("=== Test 5: Get Firmware Chunk ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        # Initiate session first
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        # Get chunk 0
        success, chunk_data, error = get_firmware_chunk_for_device(
            TEST_DEVICE_ID, TEST_FIRMWARE_VERSION, 0
        )
        
        assert success is True, f"Chunk retrieval should succeed: {error}"
        assert chunk_data is not None
        assert len(chunk_data) == 256, "Chunk should be 256 bytes"
        
        # Verify session updated
        session = ota_sessions[TEST_DEVICE_ID]
        assert session['current_chunk'] == 0
        assert session['bytes_transferred'] == 256
        
        logger.info("[PASS] Firmware chunk retrieved successfully")
    
    def test_get_multiple_chunks_sequence(self, client, temp_firmware_dir, monkeypatch):
        """Test 6: Get multiple chunks in sequence"""
        logger.info("=== Test 6: Get Multiple Chunks in Sequence ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        # Get all 4 chunks
        for chunk_index in range(4):
            success, chunk_data, error = get_firmware_chunk_for_device(
                TEST_DEVICE_ID, TEST_FIRMWARE_VERSION, chunk_index
            )
            assert success is True, f"Chunk {chunk_index} should succeed"
            assert chunk_data is not None
        
        # Verify total bytes transferred
        session = ota_sessions[TEST_DEVICE_ID]
        assert session['bytes_transferred'] == 1024, "All 1024 bytes should be transferred"
        
        logger.info("[PASS] Multiple chunks retrieved in sequence")
    
    def test_invalid_chunk_index(self, client, temp_firmware_dir, monkeypatch):
        """Test 7: Invalid chunk index handling"""
        logger.info("=== Test 7: Invalid Chunk Index ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        # Try chunk beyond total_chunks
        success, chunk_data, error = get_firmware_chunk_for_device(
            TEST_DEVICE_ID, TEST_FIRMWARE_VERSION, 100
        )
        
        assert success is False, "Invalid chunk should fail"
        assert 'invalid chunk index' in error.lower()
        
        logger.info("[PASS] Invalid chunk index rejected")
    
    def test_complete_ota_success(self, client, temp_firmware_dir, monkeypatch):
        """Test 8: Complete OTA session successfully"""
        logger.info("=== Test 8: Complete OTA Success ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        # Complete successfully
        result = complete_ota_session(TEST_DEVICE_ID, success=True)
        
        assert result is True
        assert TEST_DEVICE_ID not in ota_sessions, "Session should be removed"
        assert ota_stats['successful_updates'] == 1
        assert ota_stats['active_sessions'] == 0
        
        logger.info("[PASS] OTA session completed successfully")
    
    def test_complete_ota_failure(self, client, temp_firmware_dir, monkeypatch):
        """Test 9: Complete OTA session with failure"""
        logger.info("=== Test 9: Complete OTA Failure ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        # Complete with failure
        result = complete_ota_session(TEST_DEVICE_ID, success=False)
        
        assert result is True
        assert TEST_DEVICE_ID not in ota_sessions
        assert ota_stats['failed_updates'] == 1
        
        logger.info("[PASS] OTA failure recorded correctly")
    
    def test_get_ota_status(self, client, temp_firmware_dir, monkeypatch):
        """Test 10: Get OTA status"""
        logger.info("=== Test 10: Get OTA Status ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        status = get_ota_status(TEST_DEVICE_ID)
        
        assert status is not None
        assert 'session_id' in status or 'status' in status or 'firmware_version' in status
        
        logger.info("[PASS] OTA status retrieved")
    
    def test_multiple_device_sessions(self, client, temp_firmware_dir, monkeypatch):
        """Test 11: Multiple device OTA sessions"""
        logger.info("=== Test 11: Multiple Device Sessions ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        device1 = "DEVICE_001"
        device2 = "DEVICE_002"
        
        # Initiate sessions for both devices
        success1, sid1 = initiate_ota_session(device1, TEST_FIRMWARE_VERSION)
        success2, sid2 = initiate_ota_session(device2, TEST_FIRMWARE_VERSION)
        
        assert success1 is True
        assert success2 is True
        assert device1 in ota_sessions
        assert device2 in ota_sessions
        assert ota_stats['active_sessions'] == 2
        
        logger.info("[PASS] Multiple device sessions work independently")
    
    def test_invalid_firmware_version(self, client, temp_firmware_dir, monkeypatch):
        """Test 12: Invalid firmware version"""
        logger.info("=== Test 12: Invalid Firmware Version ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        success, error = initiate_ota_session(TEST_DEVICE_ID, "9.9.9")
        
        assert success is False
        assert 'not found' in error.lower()
        
        logger.info("[PASS] Invalid firmware version rejected")
    
    def test_get_chunk_without_session(self, client, temp_firmware_dir, monkeypatch):
        """Test 13: Get chunk without active session"""
        logger.info("=== Test 13: Get Chunk Without Session ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        success, chunk_data, error = get_firmware_chunk_for_device(
            "NONEXISTENT_DEVICE", TEST_FIRMWARE_VERSION, 0
        )
        
        assert success is False
        assert 'no active ota session' in error.lower()
        
        logger.info("[PASS] Chunk request without session rejected")
    
    def test_cancel_ota_session(self, client, temp_firmware_dir, monkeypatch):
        """Test 14: Cancel OTA session"""
        logger.info("=== Test 14: Cancel OTA Session ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        initiate_ota_session(TEST_DEVICE_ID, TEST_FIRMWARE_VERSION)
        
        # Cancel session
        result = cancel_ota_session(TEST_DEVICE_ID)
        
        assert result is True
        assert TEST_DEVICE_ID not in ota_sessions
        
        logger.info("[PASS] OTA session cancelled successfully")
    
    def test_ota_statistics_tracking(self, client, temp_firmware_dir, monkeypatch):
        """Test 15: OTA statistics tracking"""
        logger.info("=== Test 15: OTA Statistics Tracking ===")
        
        import handlers.ota_handler as ota_module
        monkeypatch.setattr(ota_module, 'FIRMWARE_DIR', Path(temp_firmware_dir))
        
        # Perform multiple operations
        initiate_ota_session("DEV1", TEST_FIRMWARE_VERSION)
        complete_ota_session("DEV1", success=True)
        
        initiate_ota_session("DEV2", TEST_FIRMWARE_VERSION)
        complete_ota_session("DEV2", success=False)
        
        stats = get_ota_stats()
        
        assert stats is not None
        assert stats['total_updates_initiated'] == 2
        assert stats['successful_updates'] == 1
        assert stats['failed_updates'] == 1
        
        logger.info("[PASS] OTA statistics tracked correctly")


if __name__ == '__main__':
    pytest.main([__file__, '-v', '--tb=short'])
