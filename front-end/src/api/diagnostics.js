/**
 * Diagnostics & Logging API Client
 * Handles diagnostic data, logs, and statistics
 */

import api from './axios';

/**
 * Get diagnostics for all devices
 * @param {number} limit - Limit per device
 * @returns {Promise} API response with diagnostics data
 */
export const getAllDiagnostics = (limit = 10) => {
  return api.get('/diagnostics', {
    params: { limit }
  });
};

/**
 * Get diagnostics for a specific device
 * @param {string} deviceId - Device ID
 * @param {number} limit - Number of records to fetch
 * @returns {Promise} API response with device diagnostics
 */
export const getDeviceDiagnostics = (deviceId, limit = 10) => {
  return api.get(`/diagnostics/${deviceId}`, {
    params: { limit }
  });
};

/**
 * Store diagnostic data for a device
 * @param {string} deviceId - Device ID
 * @param {Object} diagnosticsData - Diagnostic data object
 * @returns {Promise} API response
 */
export const storeDiagnostics = (deviceId, diagnosticsData) => {
  return api.post(`/diagnostics/${deviceId}`, diagnosticsData);
};

/**
 * Clear diagnostics for a specific device
 * @param {string} deviceId - Device ID
 * @returns {Promise} API response
 */
export const clearDeviceDiagnostics = (deviceId) => {
  return api.delete(`/diagnostics/${deviceId}`);
};

/**
 * Clear all diagnostics
 * @returns {Promise} API response
 */
export const clearAllDiagnostics = () => {
  return api.delete('/diagnostics');
};

/**
 * Get diagnostics summary
 * @param {string} deviceId - Optional device ID for device-specific summary
 * @returns {Promise} API response with summary statistics
 */
export const getDiagnosticsSummary = (deviceId = null) => {
  return api.get('/diagnostics/summary', {
    params: deviceId ? { device_id: deviceId } : {}
  });
};

/**
 * Get all system statistics
 * @returns {Promise} API response with all stats (security, OTA, commands)
 */
export const getAllStats = () => {
  return api.get('/stats');
};

/**
 * Get compression statistics
 * @returns {Promise} API response with compression stats
 */
export const getCompressionStats = () => {
  return api.get('/compression/stats');
};

/**
 * Clear compression statistics
 * @returns {Promise} API response
 */
export const clearCompressionStats = () => {
  return api.delete('/compression/stats');
};

/**
 * Get security statistics
 * @returns {Promise} API response with security stats
 */
export const getSecurityStats = () => {
  return api.get('/security/stats');
};

/**
 * Clear security statistics
 * @returns {Promise} API response
 */
export const clearSecurityStats = () => {
  return api.delete('/security/stats');
};

/**
 * Get OTA statistics
 * @returns {Promise} API response with OTA stats
 */
export const getOTAStats = () => {
  return api.get('/ota/stats');
};

/**
 * Get command statistics
 * @returns {Promise} API response with command stats
 */
export const getCommandStats = () => {
  return api.get('/commands/stats');
};

/**
 * Get system health information
 * @returns {Promise} API response with system health
 */
export const getSystemHealth = () => {
  return api.get('/health');
};

export default {
  getAllDiagnostics,
  getDeviceDiagnostics,
  storeDiagnostics,
  clearDeviceDiagnostics,
  clearAllDiagnostics,
  getDiagnosticsSummary,
  getAllStats,
  getCompressionStats,
  clearCompressionStats,
  getSecurityStats,
  clearSecurityStats,
  getOTAStats,
  getCommandStats,
  getSystemHealth
};
