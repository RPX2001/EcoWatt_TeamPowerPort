/**
 * Utilities API Client
 * Handles utility tool operations (firmware prep, key generation, benchmarking)
 */

import api from './axios';

/**
 * Prepare firmware with manifest generation
 * @param {File} file - Firmware file
 * @param {string} version - Firmware version
 * @param {string} algorithm - Compression algorithm
 * @returns {Promise} API response
 */
export const prepareFirmware = (file, version, algorithm = 'zlib') => {
  const formData = new FormData();
  formData.append('firmware', file);
  formData.append('version', version);
  formData.append('algorithm', algorithm);

  return api.post('/utilities/firmware/prepare', formData, {
    headers: {
      'Content-Type': 'multipart/form-data'
    }
  });
};

/**
 * Generate cryptographic keys
 * @param {string} keyType - Key type (PSK/HMAC)
 * @param {string} format - Output format (c_header/pem/binary)
 * @param {number} keySize - Key size in bytes
 * @returns {Promise} API response
 */
export const generateKeys = (keyType = 'PSK', format = 'c_header', keySize = 32) => {
  return api.post('/utilities/keys/generate', {
    key_type: keyType,
    format,
    key_size: keySize
  });
};

/**
 * Run compression benchmark
 * @param {number} dataSize - Test data size in bytes
 * @param {number} iterations - Number of iterations
 * @returns {Promise} API response
 */
export const runCompressionBenchmark = (dataSize = 1024, iterations = 100) => {
  return api.post('/utilities/compression/benchmark', {
    data_size: dataSize,
    iterations
  });
};

/**
 * Get utilities information
 * @returns {Promise} API response
 */
export const getUtilitiesInfo = () => {
  return api.get('/utilities/info');
};

/**
 * Get server logs with filtering
 * @param {number} limit - Number of log entries
 * @param {string} level - Log level filter (all, info, warning, error)
 * @param {string} search - Search term
 * @returns {Promise} API response
 */
export const getServerLogs = (limit = 100, level = 'all', search = '') => {
  return api.get('/utilities/logs/server', {
    params: { limit, level, search }
  });
};

/**
 * List available log files
 * @returns {Promise} API response
 */
export const listLogFiles = () => {
  return api.get('/utilities/logs/files');
};

export default {
  prepareFirmware,
  generateKeys,
  runCompressionBenchmark,
  getUtilitiesInfo,
  getServerLogs,
  listLogFiles
};
