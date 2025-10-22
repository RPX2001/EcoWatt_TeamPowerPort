/**
 * @file compression_test.h
 * @brief Header for Compression Benchmark Tool
 */

#ifndef COMPRESSION_TEST_H
#define COMPRESSION_TEST_H

/**
 * @brief Run complete compression benchmark suite
 * 
 * Tests all compression methods (Dictionary, Delta, RLE, Binary, Smart)
 * with 4 different datasets:
 * - Constant values (worst for delta, best for RLE)
 * - Linear ramp (best for delta)
 * - Realistic inverter data
 * - Random noise (worst case)
 * 
 * Verifies lossless compression and calculates accurate metrics.
 */
void runCompressionBenchmark();

/**
 * @brief Enter compression benchmark mode
 * 
 * Entry point for compression testing.
 * Call from serial command handler or setup().
 */
void enterCompressionBenchmarkMode();

#endif // COMPRESSION_TEST_H
