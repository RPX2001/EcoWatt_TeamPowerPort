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

/**
 * Test replay attack (send same payload twice)
 */
export const testReplayAttack = async (deviceId, payload) => {
  // Send the same payload twice to trigger replay detection
  const firstAttempt = await validateSecuredPayload(deviceId, payload);
  const secondAttempt = await validateSecuredPayload(deviceId, payload);
  
  return {
    firstAttempt,
    secondAttempt,
    replayDetected: firstAttempt.data.success && !secondAttempt.data.success
  };
};

/**
 * Test tampered payload (modify HMAC)
 */
export const testTamperedPayload = async (deviceId, payload) => {
  // Clone and modify the HMAC
  const tamperedPayload = {
    ...payload,
    hmac: 'tampered_' + (payload.hmac || 'invalid')
  };
  
  return validateSecuredPayload(deviceId, tamperedPayload);
};

/**
 * Test invalid HMAC
 */
export const testInvalidHMAC = async (deviceId, payload) => {
  // Send payload with completely invalid HMAC
  const invalidPayload = {
    ...payload,
    hmac: '0000000000000000000000000000000000000000000000000000000000000000'
  };
  
  return validateSecuredPayload(deviceId, invalidPayload);
};

/**
 * Test old nonce (reuse previous nonce)
 */
export const testOldNonce = async (deviceId, payload) => {
  // Send payload with old/reused nonce
  const oldNoncePayload = {
    ...payload,
    nonce: Math.floor(Date.now() / 1000) - 3600 // 1 hour ago
  };
  
  return validateSecuredPayload(deviceId, oldNoncePayload);
};
