/**
 * @file task_coordinator.h
 * @brief Task coordination and timer management for EcoWatt system
 * 
 * This module manages hardware timers and provides a token-based task scheduling
 * system for coordinating periodic tasks (polling, uploading, config checks, OTA).
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef TASK_COORDINATOR_H
#define TASK_COORDINATOR_H

#include <Arduino.h>

/**
 * @enum TaskType
 * @brief Enumeration of different task types in the system
 */
enum TaskType {
    TASK_POLL,      ///< Sensor data polling task
    TASK_UPLOAD,    ///< Cloud data upload task
    TASK_CHANGES,   ///< Configuration change check task
    TASK_OTA        ///< OTA update check task
};

/**
 * @class TaskCoordinator
 * @brief Manages hardware timers and task scheduling tokens
 * 
 * This class provides a singleton-style interface for managing four hardware timers
 * used for periodic task scheduling. It uses a token-based approach where ISRs set
 * flags that are checked in the main loop.
 */
class TaskCoordinator {
public:
    /**
     * @brief Initialize all hardware timers with specified frequencies
     * 
     * @param pollFreqUs Polling frequency in microseconds
     * @param uploadFreqUs Upload frequency in microseconds
     * @param changesFreqUs Config check frequency in microseconds
     * @param otaFreqUs OTA check frequency in microseconds
     * @return true if initialization successful, false otherwise
     */
    static bool init(uint64_t pollFreqUs, uint64_t uploadFreqUs, 
                     uint64_t changesFreqUs, uint64_t otaFreqUs);

    /**
     * @brief Update the polling frequency
     * 
     * @param newFreqUs New frequency in microseconds
     */
    static void updatePollFrequency(uint64_t newFreqUs);

    /**
     * @brief Update the upload frequency
     * 
     * @param newFreqUs New frequency in microseconds
     */
    static void updateUploadFrequency(uint64_t newFreqUs);

    /**
     * @brief Update the config check frequency
     * 
     * @param newFreqUs New frequency in microseconds
     */
    static void updateChangesFrequency(uint64_t newFreqUs);

    /**
     * @brief Update the OTA check frequency
     * 
     * @param newFreqUs New frequency in microseconds
     */
    static void updateOTAFrequency(uint64_t newFreqUs);

    /**
     * @brief Pause all tasks (disable timer alarms)
     * 
     * Used during critical operations like OTA updates
     */
    static void pauseAllTasks();

    /**
     * @brief Resume all tasks (enable timer alarms)
     */
    static void resumeAllTasks();

    /**
     * @brief Pause a specific task
     * 
     * @param task The task to pause
     */
    static void pauseTask(TaskType task);

    /**
     * @brief Resume a specific task
     * 
     * @param task The task to resume
     */
    static void resumeTask(TaskType task);

    /**
     * @brief Check if poll task is ready to run
     * 
     * @return true if poll token is set
     */
    static bool isPollReady();

    /**
     * @brief Check if upload task is ready to run
     * 
     * @return true if upload token is set
     */
    static bool isUploadReady();

    /**
     * @brief Check if config check task is ready to run
     * 
     * @return true if changes token is set
     */
    static bool isChangesReady();

    /**
     * @brief Check if OTA task is ready to run
     * 
     * @return true if OTA token is set
     */
    static bool isOTAReady();

    /**
     * @brief Reset (clear) the poll token after processing
     */
    static void resetPollToken();

    /**
     * @brief Reset (clear) the upload token after processing
     */
    static void resetUploadToken();

    /**
     * @brief Reset (clear) the changes token after processing
     */
    static void resetChangesToken();

    /**
     * @brief Reset (clear) the OTA token after processing
     */
    static void resetOTAToken();

    /**
     * @brief Get the current poll frequency
     * 
     * @return Current poll frequency in microseconds
     */
    static uint64_t getPollFrequency();

    /**
     * @brief Get the current upload frequency
     * 
     * @return Current upload frequency in microseconds
     */
    static uint64_t getUploadFrequency();

    /**
     * @brief Cleanup and deinitialize all timers
     */
    static void shutdown();

private:
    // Hardware timer handles
    static hw_timer_t* pollTimer;
    static hw_timer_t* uploadTimer;
    static hw_timer_t* changesTimer;
    static hw_timer_t* otaTimer;

    // Task ready tokens (set by ISRs)
    static volatile bool pollToken;
    static volatile bool uploadToken;
    static volatile bool changesToken;
    static volatile bool otaToken;

    // Current frequencies
    static uint64_t currentPollFreq;
    static uint64_t currentUploadFreq;
    static uint64_t currentChangesFreq;
    static uint64_t currentOTAFreq;

    // ISR handlers (must be static)
    static void IRAM_ATTR onPollTimer();
    static void IRAM_ATTR onUploadTimer();
    static void IRAM_ATTR onChangesTimer();
    static void IRAM_ATTR onOTATimer();

    // Prevent instantiation
    TaskCoordinator() = delete;
    ~TaskCoordinator() = delete;
    TaskCoordinator(const TaskCoordinator&) = delete;
    TaskCoordinator& operator=(const TaskCoordinator&) = delete;
};

#endif // TASK_COORDINATOR_H
