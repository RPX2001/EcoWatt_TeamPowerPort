import api from './axios';

/**
 * Get latest aggregated data for a device
 * @param {string} deviceId - Device identifier
 * @returns {Promise} API response with latest data
 */
export const getLatestData = (deviceId) => {
  return api.get(`/aggregation/latest/${deviceId}`);
};

/**
 * Get historical aggregated data for a device
 * @param {string} deviceId - Device identifier
 * @param {Object} params - Query parameters
 * @param {string} params.start_time - Start timestamp
 * @param {string} params.end_time - End timestamp
 * @param {number} params.limit - Maximum number of records
 * @returns {Promise} API response with historical data
 */
export const getHistoricalData = (deviceId, params = {}) => {
  return api.get(`/aggregation/historical/${deviceId}`, { params });
};

/**
 * Upload aggregated data (used by ESP32)
 * @param {string} deviceId - Device identifier
 * @param {Object} data - Aggregated sensor data
 * @returns {Promise} API response
 */
export const uploadAggregatedData = (deviceId, data) => {
  return api.post(`/aggregated/${deviceId}`, data);
};

/**
 * Export device data as CSV
 * @param {string} deviceId - Device identifier
 * @param {Object} params - Query parameters
 * @returns {Promise} API response with CSV file
 */
export const exportDataCSV = (deviceId, params = {}) => {
  return api.get(`/export/${deviceId}/csv`, { 
    params,
    responseType: 'blob'
  });
};
