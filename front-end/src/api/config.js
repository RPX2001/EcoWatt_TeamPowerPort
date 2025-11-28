import api from './axios';

/**
 * Configuration API Client
 * Following Milestone 4 Remote Configuration Message Format
 * 
 * Cloud → Device Message:
 * {
 *   "config_update": {
 *     "sampling_interval": 5,
 *     "registers": ["voltage", "current", "frequency"]
 *   }
 * }
 * 
 * Device → Cloud Acknowledgment:
 * {
 *   "config_ack": {
 *     "accepted": ["sampling_interval", "registers"],
 *     "rejected": [],
 *     "unchanged": []
 *   }
 * }
 */

/**
 * Get current configuration for a device
 * @param {string} deviceId - Device identifier
 * @returns {Promise} API response with configuration
 */
export const getConfig = (deviceId) => {
  return api.get(`/config/${deviceId}`);
};

/**
 * Update device configuration
 * Following Milestone 4 format with config_update wrapper
 * @param {string} deviceId - Device identifier
 * @param {Object} config - Configuration parameters
 * @returns {Promise} API response with config_ack
 */
export const updateConfig = (deviceId, config) => {
  return api.put(`/config/${deviceId}`, {
    config_update: config
  });
};

/**
 * Get configuration history for a device
 * @param {string} deviceId - Device identifier
 * @param {Object} params - Query parameters (limit)
 * @returns {Promise} API response with config history
 */
export const getConfigHistory = (deviceId, params = {}) => {
  return api.get(`/config/${deviceId}/history`, { params });
};
