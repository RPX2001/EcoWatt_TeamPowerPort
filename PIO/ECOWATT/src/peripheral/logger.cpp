/**
 * @file logger.cpp
 * @brief Implementation of the EcoWatt logging system
 * 
 * @author Team PowerPort
 * @date 2025-11-05
 */

#include "peripheral/logger.h"

// ============================================
// GLOBAL LOG LEVEL - Change here to control all logging
// ============================================
// LOG_LEVEL_DEBUG = 0 (all logs)
// LOG_LEVEL_INFO  = 1 (info, warnings, errors)
// LOG_LEVEL_WARN  = 2 (warnings and errors only)
// LOG_LEVEL_ERROR = 3 (errors only)
// LOG_LEVEL_NONE  = 4 (no logs)
LogLevel g_logLevel = LOG_LEVEL_DEBUG;
