"""
M4 Command Execution Tests for Flask Server
Tests the command queuing, polling, and result submission workflow

Test Coverage:
- Command queuing with validation
- Command polling by device
- Command result submission
- Command status tracking
- Command statistics
- Error handling
- Multiple devices
- Concurrent commands
"""

import pytest
import json
from datetime import datetime
from flask_server_modular import app
from handlers.command_handler import (
    command_queues,
    command_history,
    reset_command_stats,
    get_command_stats
)


@pytest.fixture
def client():
    """Create test client"""
    app.config['TESTING'] = True
    with app.test_client() as client:
        yield client


@pytest.fixture(autouse=True)
def reset_state():
    """Reset command state before each test"""
    command_queues.clear()
    command_history.clear()
    reset_command_stats()
    yield
    command_queues.clear()
    command_history.clear()


class TestCommandQueuing:
    """Test command queuing functionality"""
    
    def test_queue_valid_command(self, client):
        """Test queuing a valid command"""
        response = client.post(
            '/commands/TEST_DEVICE_1',
            json={
                'command': {
                    'action': 'write_register',
                    'target_register': 'export_power',
                    'value': 1000
                }
            }
        )
        
        assert response.status_code == 201
        data = response.get_json()
        assert data['success'] is True
        assert 'command_id' in data
        assert data['device_id'] == 'TEST_DEVICE_1'
        
        # Verify command was queued
        assert 'TEST_DEVICE_1' in command_queues
        assert len(command_queues['TEST_DEVICE_1']) == 1
        
        # Verify command in history
        cmd_id = data['command_id']
        assert cmd_id in command_history
        assert command_history[cmd_id]['command'] == 'write_register'
    
    def test_queue_command_without_parameters(self, client):
        """Test queuing a command without optional parameters"""
        response = client.post(
            '/commands/TEST_DEVICE_1',
            json={
                'command': {
                    'action': 'get_stats'
                }
            }
        )
        
        assert response.status_code == 201
        data = response.get_json()
        assert data['success'] is True
        assert 'command_id' in data
        
        # Verify parameters default to empty dict or None
        cmd_id = data['command_id']
        assert cmd_id in command_history
    
    def test_queue_command_missing_command_field(self, client):
        """Test queuing without command field"""
        response = client.post(
            '/commands/TEST_DEVICE_1',
            json={'parameters': {'power': 1000}}
        )
        
        assert response.status_code == 400
        data = response.get_json()
        assert data['success'] is False
        assert 'command' in data['error'].lower()
        assert 'command field is required' in data['error']
    
    def test_queue_command_invalid_json(self, client):
        """Test queuing with invalid content type"""
        response = client.post(
            '/commands/TEST_DEVICE_1',
            data='not json',
            content_type='text/plain'
        )
        
        # Flask returns 500 for unsupported media type in this case
        assert response.status_code in [400, 500]
        data = response.get_json()
        assert data['success'] is False
    
    def test_queue_multiple_commands(self, client):
        """Test queuing multiple commands for same device"""
        commands = [
            {'command': {'action': 'write_register', 'target_register': 'export_power', 'value': 1000}},
            {'command': {'action': 'write_register', 'target_register': 'export_power', 'value': 2000}},
            {'command': {'action': 'get_stats'}}
        ]
        
        command_ids = []
        for cmd in commands:
            response = client.post('/commands/TEST_DEVICE_1', json=cmd)
            assert response.status_code == 201
            command_ids.append(response.get_json()['command_id'])
        
        # Verify all commands in queue
        assert len(command_queues['TEST_DEVICE_1']) == 3
        
        # Verify statistics
        stats = get_command_stats()
        assert stats['total_commands'] == 3
        assert stats['pending_commands'] == 3
    
    def test_queue_commands_multiple_devices(self, client):
        """Test queuing commands for multiple devices"""
        devices = ['DEVICE_1', 'DEVICE_2', 'DEVICE_3']
        
        for device_id in devices:
            response = client.post(
                f'/commands/{device_id}',
                json={'command': {'action': 'get_stats'}}
            )
            assert response.status_code == 201
        
        # Verify each device has its own queue
        for device_id in devices:
            assert device_id in command_queues
            assert len(command_queues[device_id]) == 1


class TestCommandPolling:
    """Test command polling functionality"""
    
    def test_poll_with_pending_commands(self, client):
        """Test polling when commands are pending"""
        # Queue commands first
        for i in range(3):
            client.post(
                '/commands/TEST_DEVICE_1',
                json={'command': {'action': f'command_{i}'}}
            )
        
        # Poll commands
        response = client.get('/commands/TEST_DEVICE_1/poll')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert len(data['commands']) == 3
        
        # Verify commands have correct structure
        for cmd in data['commands']:
            assert 'command_id' in cmd
            assert 'command' in cmd
            assert 'parameters' in cmd
            assert cmd['status'] == 'executing'
    
    def test_poll_with_no_commands(self, client):
        """Test polling when no commands are pending"""
        response = client.get('/commands/TEST_DEVICE_1/poll')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert data['commands'] == []
    
    def test_poll_with_limit(self, client):
        """Test polling with limit parameter"""
        # Queue 5 commands
        for i in range(5):
            client.post(
                '/commands/TEST_DEVICE_1',
                json={'command': {'action': f'command_{i}'}}
            )
        
        # Poll with limit of 2
        response = client.get('/commands/TEST_DEVICE_1/poll?limit=2')
        
        assert response.status_code == 200
        data = response.get_json()
        assert len(data['commands']) == 2
        
        # Verify 2 commands are executing, 3 still pending
        executing = [cmd for cmd in command_queues['TEST_DEVICE_1'] 
                    if cmd['status'] == 'executing']
        pending = [cmd for cmd in command_queues['TEST_DEVICE_1'] 
                  if cmd['status'] == 'pending']
        assert len(executing) == 2
        assert len(pending) == 3
    
    def test_poll_updates_statistics(self, client):
        """Test that polling updates statistics correctly"""
        # Queue commands
        client.post('/commands/TEST_DEVICE_1', json={'command': {'action': 'cmd1'}})
        client.post('/commands/TEST_DEVICE_1', json={'command': {'action': 'cmd2'}})
        
        stats = get_command_stats()
        assert stats['pending_commands'] == 2
        assert stats['executing_commands'] == 0
        
        # Poll commands
        client.get('/commands/TEST_DEVICE_1/poll')
        
        stats = get_command_stats()
        assert stats['pending_commands'] == 0
        assert stats['executing_commands'] == 2
    
    def test_poll_only_returns_pending_commands(self, client):
        """Test that polling only returns pending commands, not executing ones"""
        # Queue 3 commands
        for i in range(3):
            client.post('/commands/TEST_DEVICE_1', json={'command': {'action': f'cmd{i}'}})
        
        # First poll - should get all 3
        response1 = client.get('/commands/TEST_DEVICE_1/poll')
        assert len(response1.get_json()['commands']) == 3
        
        # Second poll - should get 0 (all are executing now)
        response2 = client.get('/commands/TEST_DEVICE_1/poll')
        assert len(response2.get_json()['commands']) == 0


class TestCommandResultSubmission:
    """Test command result submission"""
    
    def test_submit_successful_result(self, client):
        """Test submitting a successful command result"""
        # Queue and poll command
        queue_resp = client.post(
            '/commands/TEST_DEVICE_1',
            json={'command': {'action': 'write_register', 'target_register': 'export_power', 'value': 1000}}
        )
        command_id = queue_resp.get_json()['command_id']
        
        client.get('/commands/TEST_DEVICE_1/poll')
        
        # Submit result using M4 format
        response = client.post(
            '/commands/TEST_DEVICE_1/result',
            json={
                'command_result': {
                    'command_id': command_id,
                    'status': 'success',
                    'executed_at': '2025-10-28T17:00:00Z'
                }
            }
        )
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        
        # Verify command updated in history
        assert command_history[command_id]['status'] == 'completed'
    
    def test_submit_failed_result(self, client):
        """Test submitting a failed command result"""
        # Queue and poll command
        queue_resp = client.post(
            '/commands/TEST_DEVICE_1',
            json={'command': {'action': 'invalid_command'}}
        )
        command_id = queue_resp.get_json()['command_id']
        
        client.get('/commands/TEST_DEVICE_1/poll')
        
        # Submit failure result using M4 format
        response = client.post(
            '/commands/TEST_DEVICE_1/result',
            json={
                'command_result': {
                    'command_id': command_id,
                    'status': 'failed',
                    'executed_at': '2025-10-28T17:00:00Z',
                    'error': 'Command not recognized'
                }
            }
        )
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        
        # Verify command marked as failed
        assert command_history[command_id]['status'] == 'failed'
        assert command_history[command_id]['error_message'] == 'Command not recognized'
    
    def test_submit_result_missing_command_id(self, client):
        """Test submitting result without command_id"""
        response = client.post(
            '/commands/TEST_DEVICE_1/result',
            json={'command_result': {'status': 'success'}}
        )
        
        assert response.status_code == 400
        data = response.get_json()
        assert data['success'] is False
        assert 'command_id' in data['error'].lower()
    
    def test_submit_result_invalid_command_id(self, client):
        """Test submitting result for non-existent command"""
        response = client.post(
            '/commands/TEST_DEVICE_1/result',
            json={
                'command_result': {
                    'command_id': 'nonexistent-id',
                    'status': 'success',
                    'executed_at': '2025-10-28T17:00:00Z'
                }
            }
        )
        
        assert response.status_code == 404  # Not found is correct
        data = response.get_json()
        assert data['success'] is False
    
    def test_submit_result_updates_statistics(self, client):
        """Test that result submission updates statistics"""
        # Queue and poll commands
        cmd_ids = []
        for i in range(3):
            resp = client.post('/commands/TEST_DEVICE_1', json={'command': {'action': f'cmd{i}'}})
            cmd_ids.append(resp.get_json()['command_id'])
        
        client.get('/commands/TEST_DEVICE_1/poll')
        
        # Submit successful results for 2
        for cmd_id in cmd_ids[:2]:
            client.post('/commands/TEST_DEVICE_1/result', json={
                'command_result': {
                    'command_id': cmd_id,
                    'status': 'success',
                    'executed_at': '2025-10-28T17:00:00Z'
                }
            })
        
        # Submit failed result for 1
        client.post('/commands/TEST_DEVICE_1/result', json={
            'command_result': {
                'command_id': cmd_ids[2],
                'status': 'failed',
                'executed_at': '2025-10-28T17:00:00Z'
            }
        })
        
        stats = get_command_stats()
        assert stats['completed_commands'] == 2
        assert stats['failed_commands'] == 1
        assert stats['executing_commands'] == 0


class TestCommandStatus:
    """Test command status tracking"""
    
    def test_get_command_status_pending(self, client):
        """Test getting status of pending command"""
        # Queue command
        queue_resp = client.post(
            '/commands/TEST_DEVICE_1',
            json={'command': {'action': 'test_command'}}
        )
        command_id = queue_resp.get_json()['command_id']
        
        # Get status
        response = client.get(f'/commands/status/{command_id}')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert data['status']['status'] == 'pending'
    
    def test_get_command_status_executing(self, client):
        """Test getting status of executing command"""
        # Queue and poll command
        queue_resp = client.post(
            '/commands/TEST_DEVICE_1',
            json={'command': {'action': 'test_command'}}
        )
        command_id = queue_resp.get_json()['command_id']
        
        client.get('/commands/TEST_DEVICE_1/poll')
        
        # Get status
        response = client.get(f'/commands/status/{command_id}')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['status']['status'] == 'executing'
    
    def test_get_command_status_completed(self, client):
        """Test getting status of completed command"""
        # Queue, poll, and complete command
        queue_resp = client.post(
            '/commands/TEST_DEVICE_1',
            json={'command': {'action': 'test_command'}}
        )
        command_id = queue_resp.get_json()['command_id']
        
        client.get('/commands/TEST_DEVICE_1/poll')
        client.post('/commands/TEST_DEVICE_1/result', json={
            'command_result': {
                'command_id': command_id,
                'status': 'success',
                'executed_at': '2025-10-28T17:00:00Z'
            }
        })
        
        # Get status
        response = client.get(f'/commands/status/{command_id}')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['status']['status'] == 'completed'
    
    def test_get_status_nonexistent_command(self, client):
        """Test getting status of non-existent command"""
        response = client.get('/commands/status/nonexistent-id')
        
        assert response.status_code == 404
        data = response.get_json()
        assert data['success'] is False


class TestCommandStatistics:
    """Test command statistics tracking"""
    
    def test_get_statistics_empty(self, client):
        """Test getting statistics with no commands"""
        response = client.get('/commands/stats')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        assert data['statistics']['total_commands'] == 0
    
    def test_get_statistics_with_commands(self, client):
        """Test getting statistics after processing commands"""
        # Queue 5 commands
        for i in range(5):
            client.post('/commands/TEST_DEVICE_1', json={'command': {'action': f'cmd{i}'}})
        
        # Poll 3 commands
        client.get('/commands/TEST_DEVICE_1/poll?limit=3')
        
        # Get statistics
        response = client.get('/commands/stats')
        
        assert response.status_code == 200
        data = response.get_json()
        stats = data['statistics']
        
        assert stats['total_commands'] == 5
        assert stats['pending_commands'] == 2
        assert stats['executing_commands'] == 3
    
    def test_reset_statistics(self, client):
        """Test resetting statistics"""
        # Queue some commands
        for i in range(3):
            client.post('/commands/TEST_DEVICE_1', json={'command': {'action': f'cmd{i}'}})
        
        # Reset statistics
        response = client.delete('/commands/stats')
        
        assert response.status_code == 200
        data = response.get_json()
        assert data['success'] is True
        
        # Verify statistics are reset
        stats_resp = client.get('/commands/stats')
        stats = stats_resp.get_json()['statistics']
        assert stats['total_commands'] == 0


class TestCommandWorkflow:
    """Test complete command execution workflow"""
    
    def test_full_command_workflow(self, client):
        """Test complete workflow: queue → poll → execute → report"""
        device_id = 'TEST_DEVICE_WORKFLOW'
        
        # Step 1: Queue command using M4 format
        queue_resp = client.post(
            f'/commands/{device_id}',
            json={
                'command': {
                    'action': 'write_register',
                    'target_register': 'export_power',
                    'value': 1500
                }
            }
        )
        assert queue_resp.status_code == 201
        command_id = queue_resp.get_json()['command_id']
        
        # Step 2: Device polls for commands
        poll_resp = client.get(f'/commands/{device_id}/poll')
        assert poll_resp.status_code == 200
        commands = poll_resp.get_json()['commands']
        assert len(commands) == 1
        assert commands[0]['command'] == 'write_register'
        
        # Step 3: Check status (should be executing)
        status_resp = client.get(f'/commands/status/{command_id}')
        assert status_resp.get_json()['status']['status'] == 'executing'
        
        # Step 4: Submit result using M4 format
        result_resp = client.post(
            f'/commands/{device_id}/result',
            json={
                'command_result': {
                    'command_id': command_id,
                    'status': 'success',
                    'executed_at': '2025-10-28T17:00:00Z'
                }
            }
        )
        assert result_resp.status_code == 200
        
        # Step 5: Verify final status
        final_status = client.get(f'/commands/status/{command_id}')
        assert final_status.get_json()['status']['status'] == 'completed'
    
    def test_multiple_devices_concurrent_commands(self, client):
        """Test multiple devices with concurrent commands"""
        devices = ['DEVICE_A', 'DEVICE_B', 'DEVICE_C']
        
        # Queue commands for each device
        device_commands = {}
        for device in devices:
            resp = client.post(
                f'/commands/{device}',
                json={'command': {'action': 'get_stats'}}
            )
            device_commands[device] = resp.get_json()['command_id']
        
        # Each device polls its own commands
        for device in devices:
            poll_resp = client.get(f'/commands/{device}/poll')
            commands = poll_resp.get_json()['commands']
            assert len(commands) == 1
            assert commands[0]['device_id'] == device
        
        # Each device submits results using M4 format
        for device, cmd_id in device_commands.items():
            client.post(
                f'/commands/{device}/result',
                json={
                    'command_result': {
                        'command_id': cmd_id,
                        'status': 'success',
                        'executed_at': '2025-10-28T17:00:00Z'
                    }
                }
            )
        
        # Verify all completed
        for cmd_id in device_commands.values():
            status = client.get(f'/commands/status/{cmd_id}')
            assert status.get_json()['status']['status'] == 'completed'


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
