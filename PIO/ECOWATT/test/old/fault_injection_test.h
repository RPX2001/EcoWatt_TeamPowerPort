/**
 * Fault Injection Test Header
 * 
 * Declares test functions for fault injection testing.
 * 
 * Author: EcoWatt Team
 * Date: October 22, 2025
 */

#ifndef FAULT_INJECTION_TEST_H
#define FAULT_INJECTION_TEST_H

/**
 * Run comprehensive fault injection tests.
 * 
 * Tests include:
 * - CRC corruption detection
 * - Truncated frame handling
 * - Garbage data rejection
 * - Timeout recovery
 * - Modbus exception handling
 */
void runFaultInjectionTests();

/**
 * Enter fault injection test mode.
 * Prompts for user confirmation before running tests.
 */
void enterFaultInjectionTestMode();

#endif  // FAULT_INJECTION_TEST_H
