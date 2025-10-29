/**
 * OTA (Over-The-Air) API Client
 * Handles firmware update operations
 */

import api from './axios';

/**
 * Check if firmware update is available for device
 * @param {string} deviceId - Device ID
 * @param {string} currentVersion - Current firmware version
 * @returns {Promise} API response
 */
export const checkForUpdate = (deviceId, currentVersion = '1.0.0') => {
  return api.get(`/ota/check/${deviceId}`, {
    params: { version: currentVersion }
  });
};

/**
 * Initiate OTA update session
 * @param {string} deviceId - Device ID
 * @param {string} firmwareVersion - Target firmware version
 * @returns {Promise} API response with session_id
 */
export const initiateOTA = (deviceId, firmwareVersion) => {
  return api.post(`/ota/initiate/${deviceId}`, {
    firmware_version: firmwareVersion
  });
};

/**
 * Get firmware chunk for device
 * @param {string} deviceId - Device ID
 * @param {string} version - Firmware version
 * @param {number} chunkIndex - Chunk index
 * @returns {Promise} API response with chunk data
 */
export const getFirmwareChunk = (deviceId, version, chunkIndex) => {
  return api.get(`/ota/chunk/${deviceId}`, {
    params: {
      version,
      chunk: chunkIndex
    }
  });
};

/**
 * Complete OTA session
 * @param {string} deviceId - Device ID
 * @param {string} sessionId - OTA session ID
 * @param {boolean} success - Whether OTA was successful
 * @returns {Promise} API response
 */
export const completeOTA = (deviceId, sessionId, success = true) => {
  return api.post(`/ota/complete/${deviceId}`, {
    session_id: sessionId,
    success
  });
};

/**
 * Get OTA status for device
 * @param {string} deviceId - Device ID
 * @returns {Promise} API response with OTA status
 */
export const getOTAStatus = (deviceId) => {
  return api.get(`/ota/status/${deviceId}`);
};

/**
 * Get OTA status for all devices
 * @returns {Promise} API response with all devices OTA status
 */
export const getAllOTAStatus = () => {
  return api.get('/ota/status');
};

/**
 * Get OTA statistics
 * @returns {Promise} API response with statistics
 */
export const getOTAStats = () => {
  return api.get('/ota/stats');
};

/**
 * Cancel OTA session
 * @param {string} deviceId - Device ID
 * @returns {Promise} API response
 */
export const cancelOTA = (deviceId) => {
  return api.post(`/ota/cancel/${deviceId}`);
};

/**
 * Upload firmware file
 * @param {File} file - Firmware file
 * @param {string} version - Firmware version
 * @param {Function} onProgress - Progress callback
 * @returns {Promise} API response
 */
export const uploadFirmware = (file, version, onProgress) => {
  const formData = new FormData();
  formData.append('file', file);  // Backend expects 'file', not 'firmware'
  formData.append('version', version);

  return api.post('/ota/upload', formData, {
    headers: {
      'Content-Type': 'multipart/form-data'
    },
    onUploadProgress: (progressEvent) => {
      if (onProgress) {
        const percentCompleted = Math.round(
          (progressEvent.loaded * 100) / progressEvent.total
        );
        onProgress(percentCompleted);
      }
    }
  });
};

/**
 * Get list of available firmware versions
 * @returns {Promise} API response with firmware list
 */
export const getFirmwareList = () => {
  return api.get('/ota/firmwares');
};

/**
 * Delete firmware version
 * @param {string} version - Firmware version to delete
 * @returns {Promise} API response
 */
export const deleteFirmware = (version) => {
  return api.delete(`/ota/firmware/${version}`);
};

/**
 * Get firmware manifest
 * @param {string} version - Firmware version
 * @returns {Promise} API response with manifest
 */
export const getFirmwareManifest = (version) => {
  return api.get(`/ota/firmware/${version}/manifest`);
};

export default {
  checkForUpdate,
  initiateOTA,
  getFirmwareChunk,
  completeOTA,
  getOTAStatus,
  getAllOTAStatus,
  getOTAStats,
  cancelOTA,
  uploadFirmware,
  getFirmwareList,
  deleteFirmware,
  getFirmwareManifest
};
