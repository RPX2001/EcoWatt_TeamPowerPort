/**
 * Fault Injection API Client
 * 
 * Supports two backends:
 * 1. Inverter SIM API - For Modbus/Inverter faults
 * 2. Local Flask - For application-level faults
 */

import apiClient from './axios';

/**
 * Inject a fault for testing
 * @param {Object} faultData - Fault configuration
 * @returns {Promise} API response
 */
export const injectFault = (faultData) => {
  return apiClient.post('/fault/inject', faultData);
};

/**
 * Inject Inverter SIM fault (Modbus-related)
 * @param {Object} config - Fault configuration
 * @param {number} config.slaveAddress - Modbus slave address (default: 1)
 * @param {number} config.functionCode - Modbus function code (default: 3)
 * @param {string} config.errorType - EXCEPTION|CRC_ERROR|CORRUPT|PACKET_DROP|DELAY
 * @param {number} [config.exceptionCode] - Exception code (required for EXCEPTION)
 * @param {number} [config.delayMs] - Delay in milliseconds (required for DELAY)
 * @returns {Promise} API response
 */
export const injectInverterFault = (config) => {
  return apiClient.post('/fault/inject', config);
};

/**
 * Inject local fault (application-level)
 * @param {Object} config - Fault configuration
 * @param {string} config.fault_type - network|command|ota
 * @param {string} config.target - Target component
 * @param {number} config.duration - Duration in seconds
 * @returns {Promise} API response
 */
export const injectLocalFault = (config) => {
  return apiClient.post('/fault/inject', config);
};

/**
 * Get status of active faults
 * @returns {Promise} API response with active faults
 */
export const getFaultStatus = () => {
  return apiClient.get('/fault/status');
};

/**
 * Clear all active faults or specific fault
 * @param {string} [faultId] - Optional specific fault ID to clear
 * @returns {Promise} API response
 */
export const clearFaults = (faultId = null) => {
  const params = faultId ? { fault_id: faultId } : {};
  return apiClient.post('/fault/clear', null, { params });
};

/**
 * Get OTA fault injection status
 * @returns {Promise} API response with OTA fault status
 */
export const getOTAFaultStatus = () => {
  return apiClient.get('/ota/test/status');
};

/**
 * Disable/Clear OTA fault injection
 * @returns {Promise} API response
 */
export const clearOTAFaults = () => {
  return apiClient.post('/ota/test/disable');
};

/**
 * Get network fault injection status
 * @returns {Promise} API response with network fault status
 */
export const getNetworkFaultStatus = () => {
  return apiClient.get('/fault/network/status');
};

/**
 * Disable/Clear network fault injection
 * @returns {Promise} API response
 */
export const clearNetworkFaults = () => {
  return apiClient.post('/fault/network/clear');
};

/**
 * Get available fault types
 * @returns {Promise} API response with fault types
 */
export const getFaultTypes = () => {
  return apiClient.get('/fault/types');
};

/**
 * Predefined fault configurations
 */
export const FAULT_PRESETS = {
  // Inverter SIM faults
  modbus_exception: {
    name: 'Modbus Exception',
    config: {
      slaveAddress: 1,
      functionCode: 3,
      errorType: 'EXCEPTION',
      exceptionCode: 2  // Illegal Data Address
    }
  },
  crc_error: {
    name: 'CRC Error',
    config: {
      slaveAddress: 1,
      functionCode: 3,
      errorType: 'CRC_ERROR'
    }
  },
  corrupt_response: {
    name: 'Corrupt Response',
    config: {
      slaveAddress: 1,
      functionCode: 3,
      errorType: 'CORRUPT'
    }
  },
  packet_drop: {
    name: 'Packet Drop',
    config: {
      slaveAddress: 1,
      functionCode: 3,
      errorType: 'PACKET_DROP'
    }
  },
  response_delay: {
    name: 'Response Delay (500ms)',
    config: {
      slaveAddress: 1,
      functionCode: 3,
      errorType: 'DELAY',
      delayMs: 500
    }
  },
  
  // Local faults
  network_failure: {
    name: 'Network Upload Failure',
    config: {
      fault_type: 'network',
      target: 'upload',
      duration: 60
    }
  },
  command_timeout: {
    name: 'Command Timeout',
    config: {
      fault_type: 'command',
      target: 'execution',
      duration: 60
    }
  },
  ota_failure: {
    name: 'OTA Download Failure',
    config: {
      fault_type: 'ota',
      target: 'download',
      duration: 60
    }
  }
};

/**
 * Modbus exception codes
 */
export const MODBUS_EXCEPTION_CODES = {
  0x01: 'Illegal Function',
  0x02: 'Illegal Data Address',
  0x03: 'Illegal Data Value',
  0x04: 'Server Device Failure',
  0x05: 'Acknowledge',
  0x06: 'Server Device Busy',
  0x08: 'Memory Parity Error',
  0x0A: 'Gateway Path Unavailable',
  0x0B: 'Gateway Target Device Failed to Respond'
};

/**
 * Get recovery events for a specific device (Milestone 5)
 * @param {string} deviceId - Device ID
 * @param {Object} [params] - Query parameters
 * @param {number} [params.limit] - Max events to return
 * @param {number} [params.offset] - Pagination offset
 * @param {string} [params.fault_type] - Filter by fault type
 * @returns {Promise} API response with recovery events
 */
export const getRecoveryEvents = (deviceId, params = {}) => {
  return apiClient.get(`/fault/recovery/${deviceId}`, { params });
};

/**
 * Get all recovery events across all devices (Milestone 5)
 * @returns {Promise} API response with all recovery events and statistics
 */
export const getAllRecoveryEvents = () => {
  return apiClient.get('/fault/recovery/all');
};

/**
 * Clear recovery events for a device or all devices (Milestone 5)
 * @param {string} [deviceId] - Optional device ID to clear (omit to clear all)
 * @returns {Promise} API response
 */
export const clearRecoveryEvents = (deviceId = null) => {
  const data = deviceId ? { device_id: deviceId } : {};
  return apiClient.post('/fault/recovery/clear', data);
};

/**
 * Get fault injection history from database (Milestone 5)
 * @param {string} [deviceId] - Optional device ID to filter
 * @param {number} [limit=50] - Number of records to return
 * @returns {Promise} API response with injection history
 */
export const getInjectionHistory = (deviceId = null, limit = 50) => {
  const params = { limit };
  if (deviceId) {
    params.device_id = deviceId;
  }
  return apiClient.get('/fault/injection/history', { params });
};

/**
 * Simulate a recovery event (for testing)
 * @param {Object} event - Recovery event data
 * @param {string} event.device_id - Device ID
 * @param {number} event.timestamp - Unix timestamp
 * @param {string} event.fault_type - Fault type
 * @param {string} event.recovery_action - Recovery action taken
 * @param {boolean} event.success - Recovery success status
 * @param {string} [event.details] - Additional details
 * @returns {Promise} API response
 */
export const postRecoveryEvent = (event) => {
  return apiClient.post('/fault/recovery', event);
};

export default {
  injectFault,
  injectInverterFault,
  injectLocalFault,
  getFaultStatus,
  clearFaults,
  getFaultTypes,
  getOTAFaultStatus,
  clearOTAFaults,
  getNetworkFaultStatus,
  clearNetworkFaults,
  getRecoveryEvents,
  getAllRecoveryEvents,
  clearRecoveryEvents,
  getInjectionHistory,
  postRecoveryEvent,
  FAULT_PRESETS,
  MODBUS_EXCEPTION_CODES
};
