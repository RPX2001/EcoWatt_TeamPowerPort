import api from './axios';

/**
 * Commands API Client
 * Following Milestone 4 Command Execution Protocol Specification
 * 
 * Cloud → Device Command Message:
 * {
 *   "command": {
 *     "action": "write_register",
 *     "target_register": "export_power",
 *     "value": 50
 *   }
 * }
 * 
 * Device → Cloud Execution Result:
 * {
 *   "command_result": {
 *     "status": "success",
 *     "executed_at": "2025-09-04T14:12:00Z"
 *   }
 * }
 */

/**
 * Send a command to a device
 * Command is queued and executed at next communication window
 * @param {string} deviceId - Device identifier
 * @param {Object} command - Command object with action and parameters
 * @returns {Promise} API response with command_id
 */
export const sendCommand = (deviceId, command) => {
  return api.post(`/commands/${deviceId}`, command);
};

/**
 * Get pending commands for a device
 * @param {string} deviceId - Device identifier
 * @returns {Promise} API response with pending commands list
 */
export const getPendingCommands = (deviceId) => {
  return api.get(`/commands/${deviceId}/pending`);
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
