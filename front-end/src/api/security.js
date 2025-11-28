/**
 * Security API Client
 * Handles security testing and validation endpoints
 */

import apiClient from './axios';

/**
 * Validate a secured payload
 */
export const validateSecuredPayload = async (deviceId, payload) => {
  return apiClient.post(`/security/validate/${deviceId}`, payload);
};

/**
 * Get security statistics
 */
export const getSecurityStats = async () => {
  return apiClient.get('/security/stats');
};

/**
 * Reset security statistics
 */
export const resetSecurityStats = async () => {
  return apiClient.delete('/security/stats');
};

/**
 * Clear nonces for a device
 */
export const clearDeviceNonces = async (deviceId) => {
  return apiClient.delete(`/security/nonces/${deviceId}`);
};

/**
 * Clear all nonces
 */
export const clearAllNonces = async () => {
  return apiClient.delete('/security/nonces');
};

/**
 * Get device security info
 */
export const getDeviceSecurityInfo = async (deviceId) => {
  return apiClient.get(`/security/device/${deviceId}`);
};

// ============================================================================
// SECURITY FAULT INJECTION API (Milestone 5)
// ============================================================================

/**
 * Get available security fault types
 */
export const getSecurityFaultTypes = async () => {
  return apiClient.get('/fault/security/types');
};

/**
 * Get current security fault injection status
 */
export const getSecurityFaultStatus = async () => {
  return apiClient.get('/fault/security/status');
};

/**
 * Inject a security fault for testing
 * @param {string} faultType - Type of fault (replay, invalid_hmac, tampered_payload, old_nonce, missing_nonce, invalid_format)
 * @param {string} targetDevice - Device ID to test against
 */
export const injectSecurityFault = async (faultType, targetDevice) => {
  return apiClient.post('/fault/security/inject', {
    fault_type: faultType,
    target_device: targetDevice
  });
};

/**
 * Clear/reset security fault injection state
 */
export const clearSecurityFaults = async () => {
  return apiClient.post('/fault/security/clear');
};

// ============================================================================
// INDIVIDUAL SECURITY TEST FUNCTIONS
// ============================================================================

/**
 * Test replay attack detection
 * Sends the same payload twice - second attempt should fail
 */
export const testReplayAttack = async (deviceId) => {
  return injectSecurityFault('replay', deviceId);
};

/**
 * Test invalid HMAC detection
 * Sends payload with all-zeros HMAC
 */
export const testInvalidHMAC = async (deviceId) => {
  return injectSecurityFault('invalid_hmac', deviceId);
};

/**
 * Test tampered payload detection
 * Modifies payload after HMAC calculation
 */
export const testTamperedPayload = async (deviceId) => {
  return injectSecurityFault('tampered_payload', deviceId);
};

/**
 * Test old nonce rejection
 * Sends payload with nonce from 1 hour ago
 */
export const testOldNonce = async (deviceId) => {
  return injectSecurityFault('old_nonce', deviceId);
};

/**
 * Test missing nonce handling
 * Sends payload without nonce field
 */
export const testMissingNonce = async (deviceId) => {
  return injectSecurityFault('missing_nonce', deviceId);
};

/**
 * Test invalid format handling
 * Sends completely malformed security payload
 */
export const testInvalidFormat = async (deviceId) => {
  return injectSecurityFault('invalid_format', deviceId);
};
