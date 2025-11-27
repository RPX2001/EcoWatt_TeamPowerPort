/**
 * @file deadline_monitor.cpp
 * @brief Implementation of intelligent deadline miss tracking
 * 
 * @date November 28, 2025
 */

#include "application/deadline_monitor.h"
#include "peripheral/logger.h"

DeadlineMonitor::DeadlineMonitor() 
    : writeIndex(0)
    , lastNetworkIssue(0)
    , totalLifetimeMisses(0)
    , networkRelatedMisses(0) {
    // Initialize all timestamps to 0
    for (int i = 0; i < MAX_DEADLINE_MISSES; i++) {
        missTimestamps[i] = 0;
    }
}

void DeadlineMonitor::recordMiss(bool isNetworkRelated) {
    uint32_t now = millis();
    
    // Track network-related issues
    if (isNetworkRelated) {
        lastNetworkIssue = now;
        networkRelatedMisses++;
    }
    
    // Store timestamp in circular buffer
    missTimestamps[writeIndex] = now;
    writeIndex = (writeIndex + 1) % MAX_DEADLINE_MISSES;
    
    // Increment lifetime counter for diagnostics
    totalLifetimeMisses++;
    
    // Log with context
    uint8_t recentMisses = getRecentMisses();
    if (isNetworkRelated) {
        LOG_WARN(LOG_TAG_WATCHDOG, "Deadline miss (network-related) - %d recent misses in window", 
                 recentMisses);
    } else {
        LOG_WARN(LOG_TAG_WATCHDOG, "Deadline miss (hardware/software) - %d recent misses in window", 
                 recentMisses);
    }
}

bool DeadlineMonitor::shouldRestart() {
    uint32_t now = millis();
    uint8_t recentMisses = getRecentMisses();
    
    // Apply grace period if recent network issue
    if (isInGracePeriod()) {
        // During grace period, allow 2x normal threshold
        uint8_t gracePeriodThreshold = MAX_DEADLINE_MISSES * 2;
        
        if (recentMisses > gracePeriodThreshold) {
            LOG_ERROR(LOG_TAG_WATCHDOG, 
                     "CRITICAL: %d deadline misses exceeds grace period threshold (%d)",
                     recentMisses, gracePeriodThreshold);
            return true;
        }
        
        LOG_DEBUG(LOG_TAG_WATCHDOG, 
                 "Grace period active: %d misses (threshold: %d)",
                 recentMisses, gracePeriodThreshold);
        return false;
    }
    
    // Normal operation - standard threshold
    if (recentMisses > MAX_DEADLINE_MISSES) {
        LOG_ERROR(LOG_TAG_WATCHDOG, 
                 "CRITICAL: Excessive deadline misses (%d > %d) in %d sec window!",
                 recentMisses, MAX_DEADLINE_MISSES, EVALUATION_WINDOW_MS / 1000);
        
        // Log breakdown for debugging
        LOG_ERROR(LOG_TAG_WATCHDOG, 
                 "Lifetime: %lu total, %lu network-related",
                 totalLifetimeMisses, networkRelatedMisses);
        
        return true;
    }
    
    return false;
}

void DeadlineMonitor::onNetworkRestored() {
    uint32_t now = millis();
    uint32_t cutoff = now - NETWORK_ISSUE_CUTOFF_MS;
    
    uint8_t clearedCount = 0;
    
    // Clear misses older than cutoff (likely network-related)
    for (int i = 0; i < MAX_DEADLINE_MISSES; i++) {
        if (missTimestamps[i] > 0 && missTimestamps[i] < cutoff) {
            missTimestamps[i] = 0;
            clearedCount++;
        }
    }
    
    LOG_INFO(LOG_TAG_WATCHDOG, 
            "Network restored - cleared %d old deadline misses (cutoff: %d sec)",
            clearedCount, NETWORK_ISSUE_CUTOFF_MS / 1000);
    
    // Log current status
    uint8_t remaining = getRecentMisses();
    LOG_INFO(LOG_TAG_WATCHDOG, "Recent deadline misses after cleanup: %d", remaining);
}

uint8_t DeadlineMonitor::getRecentMisses() const {
    uint32_t now = millis();
    uint8_t count = 0;
    
    // Count misses within evaluation window
    for (int i = 0; i < MAX_DEADLINE_MISSES; i++) {
        if (missTimestamps[i] > 0 && (now - missTimestamps[i]) < EVALUATION_WINDOW_MS) {
            count++;
        }
    }
    
    return count;
}

void DeadlineMonitor::reset() {
    writeIndex = 0;
    lastNetworkIssue = 0;
    totalLifetimeMisses = 0;
    networkRelatedMisses = 0;
    
    for (int i = 0; i < MAX_DEADLINE_MISSES; i++) {
        missTimestamps[i] = 0;
    }
    
    LOG_INFO(LOG_TAG_WATCHDOG, "Deadline monitor reset");
}

bool DeadlineMonitor::isInGracePeriod() const {
    if (lastNetworkIssue == 0) {
        return false;
    }
    
    uint32_t now = millis();
    return (now - lastNetworkIssue) < NETWORK_GRACE_PERIOD_MS;
}
