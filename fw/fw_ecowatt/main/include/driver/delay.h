/**
 * @file delay.h
 * @brief Wrapper for delay functionality.
 *
 * @author Yasith
 * @version 1.0
 * @date 09-09-2025
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-09-09) Original file.
 */

#ifndef DELAY_H
#define DELAY_H

#include "esp32.h"

#define delay_wait(ms) esp32_wait(ms)

#endif