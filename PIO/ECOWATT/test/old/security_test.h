/**
 * @file security_test.h
 * @brief Header for Security Testing Utilities
 */

#ifndef SECURITY_TEST_H
#define SECURITY_TEST_H

/**
 * @brief Run all security tests
 * 
 * Executes a comprehensive suite of security tests including:
 * - Valid payload acceptance
 * - Replay attack detection
 * - Tampered MAC rejection
 * - Invalid JSON handling
 * - Missing field validation
 * - Old nonce rejection
 */
void runAllSecurityTests();

/**
 * @brief Enter security test mode
 * 
 * Initializes security layer and runs all tests.
 * Call this from serial command handler or setup().
 */
void enterSecurityTestMode();

#endif // SECURITY_TEST_H
