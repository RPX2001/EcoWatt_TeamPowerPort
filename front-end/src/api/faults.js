/**
 * Fault Injection API Client
 * 
 * Supports two backends:
 * 1. Inverter SIM API - For Modbus/Inverter faults
 * 2. Local Flask - For application-level faults
 */

import apiClient from './client';

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
 * @param {string} config.fault_type - network|mqtt|command|ota
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
  mqtt_disconnect: {
    name: 'MQTT Disconnect',
    config: {
      fault_type: 'mqtt',
      target: 'connection',
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

export default {
  injectFault,
  injectInverterFault,
  injectLocalFault,
  getFaultStatus,
  clearFaults,
  getFaultTypes,
  FAULT_PRESETS,
  MODBUS_EXCEPTION_CODES
};
