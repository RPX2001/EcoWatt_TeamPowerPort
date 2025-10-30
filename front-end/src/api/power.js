import api from './axios';

/**
 * Power Management API Client
 * Handles power configuration and energy reporting
 * 
 * Power Techniques Bitmask:
 * - 0x01 (bit 0): WiFi Modem Sleep
 * - 0x02 (bit 1): CPU Frequency Scaling
 * - 0x04 (bit 2): Light Sleep
 * - 0x08 (bit 3): Peripheral Gating
 * 
 * Example: techniques = 0x05 means WiFi Modem Sleep + Light Sleep enabled
 */

/**
 * Get current power management configuration for a device
 * @param {string} deviceId - Device identifier
 * @returns {Promise} API response with power configuration
 * Response format:
 * {
 *   device_id: "ESP32_TEST_DEVICE",
 *   power_management: {
 *     enabled: false,
 *     techniques: "0x01",
 *     techniques_list: ["wifi_modem_sleep"],
 *     energy_poll_freq: 300000
 *   }
 * }
 */
export const getPowerConfig = (deviceId) => {
  return api.get(`/power/${deviceId}`);
};

/**
 * Update power management configuration
 * @param {string} deviceId - Device identifier
 * @param {Object} config - Power configuration
 * @param {boolean} config.enabled - Enable/disable power management
 * @param {number} config.techniques - Bitmask of enabled techniques (0x00-0x0F)
 * @param {number} config.energy_poll_freq - Energy reporting frequency in milliseconds
 * @returns {Promise} API response with updated configuration
 */
export const updatePowerConfig = (deviceId, config) => {
  return api.post(`/power/${deviceId}`, config);
};

/**
 * Get energy report history for a device
 * @param {string} deviceId - Device identifier
 * @param {string} period - Time period ('1h', '24h', '7d', '30d')
 * @param {number} limit - Maximum number of records
 * @returns {Promise} API response with energy reports
 * Response format:
 * {
 *   device_id: "ESP32_TEST_DEVICE",
 *   period: "24h",
 *   count: 48,
 *   reports: [
 *     {
 *       timestamp: "2025-10-30T12:00:00+05:30",
 *       enabled: true,
 *       techniques: "0x05",
 *       techniques_list: ["wifi_modem_sleep", "light_sleep"],
 *       avg_current_ma: 123.45,
 *       energy_saved_mah: 67.89,
 *       uptime_ms: 3600000,
 *       time_distribution: {
 *         high_perf_ms: 900000,
 *         normal_ms: 1800000,
 *         low_power_ms: 600000,
 *         sleep_ms: 300000
 *       }
 *     },
 *     ...
 *   ]
 * }
 */
export const getEnergyHistory = (deviceId, period = '24h', limit = 100) => {
  return api.get(`/power/energy/${deviceId}`, {
    params: { period, limit }
  });
};

/**
 * Convert techniques bitmask to array of technique names
 * @param {number} techniques - Bitmask value (0x00-0x0F)
 * @returns {Array<string>} Array of technique names
 */
export const decodeTechniques = (techniques) => {
  const result = [];
  if (techniques & 0x01) result.push('wifi_modem_sleep');
  if (techniques & 0x02) result.push('cpu_freq_scaling');
  if (techniques & 0x04) result.push('light_sleep');
  if (techniques & 0x08) result.push('peripheral_gating');
  return result;
};

/**
 * Convert array of technique names to bitmask
 * @param {Array<string>} techniquesList - Array of technique names
 * @returns {number} Bitmask value
 */
export const encodeTechniques = (techniquesList) => {
  let bitmask = 0;
  if (techniquesList.includes('wifi_modem_sleep')) bitmask |= 0x01;
  if (techniquesList.includes('cpu_freq_scaling')) bitmask |= 0x02;
  if (techniquesList.includes('light_sleep')) bitmask |= 0x04;
  if (techniquesList.includes('peripheral_gating')) bitmask |= 0x08;
  return bitmask;
};

/**
 * Get human-readable technique names
 */
export const TECHNIQUE_NAMES = {
  wifi_modem_sleep: 'WiFi Modem Sleep',
  cpu_freq_scaling: 'CPU Frequency Scaling',
  light_sleep: 'Light Sleep',
  peripheral_gating: 'Peripheral Gating'
};

/**
 * Get technique descriptions
 */
export const TECHNIQUE_DESCRIPTIONS = {
  wifi_modem_sleep: 'Puts WiFi modem to sleep between packets, reducing power by ~20-30mA',
  cpu_freq_scaling: 'Dynamically adjusts CPU frequency based on load (Future feature)',
  light_sleep: 'Enters light sleep mode during idle periods (Future feature)',
  peripheral_gating: 'Powers down unused peripherals (Future feature)'
};
