import api from './axios';

/**
 * Commands API Client
 * Following STRICT Milestone 4 Command Execution Protocol
 * 
 * STANDARDIZED ENDPOINTS (matching Flask & ESP32):
 * - POST   /commands/<device_id>        - Queue command
 * - GET    /commands/<device_id>/poll   - Poll for pending commands
 * - POST   /commands/<device_id>/result - Submit execution result
 * - GET    /commands/status/<command_id> - Get command status
 * - GET    /commands/<device_id>/history - Get command history
 * - GET    /commands/stats               - Get statistics
 */

/**
 * Send a command to a device
 * Command is queued and executed at next communication window
 * 
 * MILESTONE 4 FORMAT:
 * Request: {
 *   "command": {
 *     "action": "write_register",
 *     "target_register": "export_power",
 *     "value": 50
 *   }
 * }
 * Response: { "success": true, "command_id": "uuid", ... }
 * 
 * @param {string} deviceId - Device identifier
 * @param {string} action - Command action (e.g., "write_register", "get_stats")
 * @param {string} targetRegister - Target register name (optional)
 * @param {*} value - Value to write (optional)
 * @returns {Promise} API response with command_id
 */
export const sendCommand = (deviceId, action, targetRegister = null, value = null) => {
  const commandPayload = {
    command: {
      action: action
    }
  };
  
  // Add optional M4 fields only if provided
  if (targetRegister !== null) {
    commandPayload.command.target_register = targetRegister;
  }
  if (value !== null) {
    commandPayload.command.value = value;
  }
  
  return api.post(`/commands/${deviceId}`, commandPayload);
};

/**
 * Poll for pending commands - STANDARD ENDPOINT
 * Used by ESP32, frontend, and tests
 * 
 * @param {string} deviceId - Device identifier
 * @param {number} limit - Max commands to return (default: 10)
 * @returns {Promise} API response with commands array
 */
export const pollCommands = (deviceId, limit = 10) => {
  return api.get(`/commands/${deviceId}/poll`, { params: { limit } });
};

/**
 * Submit command execution result
 * 
 * MILESTONE 4 FORMAT:
 * Request: {
 *   "command_result": {
 *     "command_id": "uuid",
 *     "status": "success",
 *     "executed_at": "2025-10-28T17:00:00Z"
 *   }
 * }
 * 
 * @param {string} deviceId - Device identifier
 * @param {string} commandId - Command identifier
 * @param {string} status - "success" or "failed"
 * @param {string} executedAt - ISO timestamp
 * @param {string} error - Error message (optional, for failures)
 * @returns {Promise} API response
 */
export const submitCommandResult = (deviceId, commandId, status, executedAt, error = null) => {
  const resultPayload = {
    command_result: {
      command_id: commandId,
      status: status,
      executed_at: executedAt
    }
  };
  
  if (error) {
    resultPayload.command_result.error = error;
  }
  
  return api.post(`/commands/${deviceId}/result`, resultPayload);
};

/**
 * Get command status by ID
 * @param {string} commandId - Command identifier
 * @returns {Promise} API response with command status
 */
export const getCommandStatus = (commandId) => {
  return api.get(`/commands/status/${commandId}`);
};

/**
 * Get command execution history for a device
 * @param {string} deviceId - Device identifier
 * @param {number} limit - Number of history entries (default: 20, max: 100)
 * @returns {Promise} API response with command history
 */
export const getCommandHistory = (deviceId, limit = 20) => {
  return api.get(`/commands/${deviceId}/history`, { params: { limit } });
};

/**
 * Get command statistics across all devices
 * @returns {Promise} API response with statistics
 */
export const getCommandStats = () => {
  return api.get('/commands/stats');
};
