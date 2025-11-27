/**
 * @file deadline_monitor.h
 * @brief Intelligent deadline miss tracking with sliding window and contextual reset
 * 
 * This monitor prevents false-positive restarts caused by transient network issues
 * by implementing:
 * - Sliding time window (only recent misses count)
 * - Contextual awareness (distinguishes network vs hardware issues)
 * - Grace period after network recovery
 * 
 * @date November 28, 2025
 */

#ifndef DEADLINE_MONITOR_H
#define DEADLINE_MONITOR_H

#include <Arduino.h>
#include "system_config.h"

/**
 * @brief Tracks deadline misses with intelligent reset logic
 * 
 * Uses circular buffer to track miss timestamps in a sliding window.
 * Only misses within the evaluation window count toward restart threshold.
 */
class DeadlineMonitor {
private:
    uint32_t missTimestamps[MAX_DEADLINE_MISSES];  // Circular buffer of miss times
    uint8_t writeIndex;                             // Next write position in buffer
    uint32_t lastNetworkIssue;                      // When last network problem occurred
    uint32_t totalLifetimeMisses;                   // Diagnostic counter (not used for restart)
    uint32_t networkRelatedMisses;                  // Count of network-caused misses
    
    const uint32_t EVALUATION_WINDOW_MS = 300000;   // 5 minutes
    const uint32_t NETWORK_GRACE_PERIOD_MS = 60000; // 1 minute grace after network recovery
    const uint32_t NETWORK_ISSUE_CUTOFF_MS = 120000; // Clear misses older than 2 minutes on recovery
    
public:
    /**
     * @brief Initialize deadline monitor
     */
    DeadlineMonitor();
    
    /**
     * @brief Record a deadline miss
     * @param isNetworkRelated True if miss was caused by network/WiFi issue
     */
    void recordMiss(bool isNetworkRelated = false);
    
    /**
     * @brief Check if system should restart due to excessive deadline misses
     * @return True if restart threshold exceeded
     * 
     * Uses sliding window and applies grace period if recent network issues
     */
    bool shouldRestart();
    
    /**
     * @brief Call when network connectivity is restored
     * 
     * Clears old misses that were likely network-related
     */
    void onNetworkRestored();
    
    /**
     * @brief Get count of deadline misses in evaluation window
     * @return Number of recent misses
     */
    uint8_t getRecentMisses() const;
    
    /**
     * @brief Get total lifetime misses (for diagnostics)
     * @return Total misses since boot
     */
    uint32_t getLifetimeMisses() const { return totalLifetimeMisses; }
    
    /**
     * @brief Get network-related miss count
     * @return Network-caused misses
     */
    uint32_t getNetworkMisses() const { return networkRelatedMisses; }
    
    /**
     * @brief Reset all counters (for testing)
     */
    void reset();
    
    /**
     * @brief Check if currently in grace period after network recovery
     * @return True if grace period active
     */
    bool isInGracePeriod() const;
};

#endif // DEADLINE_MONITOR_H
