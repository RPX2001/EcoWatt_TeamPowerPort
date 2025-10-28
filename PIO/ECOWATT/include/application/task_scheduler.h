/**
 * @file task_scheduler.h
 * @brief Real-Time Task Scheduler for EcoWatt System
 * 
 * Implements a priority-based task queue to prevent timer conflicts,
 * deadlocks, and resource contention between concurrent operations.
 * 
 * Priority Levels:
 *   1. CRITICAL (Poll, Upload) - No delays allowed
 *   2. HIGH (Commands) - Can wait for CRITICAL tasks
 *   3. MEDIUM (Config checks) - Can be deferred
 *   4. LOW (FOTA) - Exclusive mode, waits for all tasks
 * 
 * @author Team PowerPort
 * @date 2025-10-28
 */

#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <Arduino.h>

// Maximum queue size
#define MAX_TASK_QUEUE_SIZE 16

/**
 * @enum ScheduledTaskType
 * @brief Types of tasks that can be scheduled
 */
enum ScheduledTaskType {
    SCHED_TASK_NONE = 0,
    SCHED_TASK_POLL_SENSORS,      // Read from Inverter SIM
    SCHED_TASK_UPLOAD_DATA,       // Upload compressed data
    SCHED_TASK_CHECK_COMMANDS,    // Poll for pending commands
    SCHED_TASK_CHECK_CONFIG,      // Check for config updates
    SCHED_TASK_CHECK_FOTA         // Check for firmware updates
};

/**
 * @enum TaskPriority
 * @brief Priority levels for task execution
 */
enum TaskPriority {
    PRIORITY_CRITICAL = 0,  // Poll and Upload (no delays)
    PRIORITY_HIGH = 1,      // Commands
    PRIORITY_MEDIUM = 2,    // Config checks
    PRIORITY_LOW = 3        // FOTA (exclusive)
};

/**
 * @enum SystemState
 * @brief Current state of the system
 */
enum SystemState {
    STATE_IDLE,             // No tasks running
    STATE_POLLING,          // Reading from Inverter SIM
    STATE_UPLOADING,        // Uploading data to cloud
    STATE_COMMANDING,       // Executing commands
    STATE_CONFIG_CHECK,     // Checking config updates
    STATE_FOTA              // Firmware update in progress (EXCLUSIVE)
};

/**
 * @struct Task
 * @brief Represents a scheduled task
 */
struct Task {
    ScheduledTaskType type;
    TaskPriority priority;
    unsigned long queuedTime;  // When task was queued (for timeout detection)
    
    Task() : type(SCHED_TASK_NONE), priority(PRIORITY_MEDIUM), queuedTime(0) {}
    Task(ScheduledTaskType t, TaskPriority p) : type(t), priority(p), queuedTime(millis()) {}
};

/**
 * @class TaskScheduler
 * @brief Priority-based real-time task scheduler
 * 
 * Prevents resource contention and deadlocks by ensuring only one
 * operation runs at a time, with strict priority ordering.
 */
class TaskScheduler {
public:
    /**
     * @brief Initialize the task scheduler
     */
    static void init();
    
    /**
     * @brief Queue a new task for execution
     * 
     * @param type Type of task to queue
     * @param priority Priority level
     * @return true if task was queued, false if queue is full
     */
    static bool queueTask(ScheduledTaskType type, TaskPriority priority);
    
    /**
     * @brief Get the next task to execute based on priority
     * 
     * @return Next task, or SCHED_TASK_NONE if queue is empty or system is blocked
     */
    static Task getNextTask();
    
    /**
     * @brief Mark the current task as started
     * 
     * @param type Task type that is starting
     */
    static void taskStarted(ScheduledTaskType type);
    
    /**
     * @brief Mark the current task as completed
     */
    static void taskCompleted();
    
    /**
     * @brief Check if a task type is already queued
     * 
     * @param type Task type to check
     * @return true if task is in queue
     */
    static bool isTaskQueued(ScheduledTaskType type);
    
    /**
     * @brief Check if system is currently busy
     * 
     * @return true if a task is currently executing
     */
    static bool isBusy();
    
    /**
     * @brief Get current system state
     * 
     * @return Current SystemState
     */
    static SystemState getCurrentState();
    
    /**
     * @brief Check if FOTA can start (all critical tasks done)
     * 
     * @return true if safe to start FOTA
     */
    static bool canStartFOTA();
    
    /**
     * @brief Force clear all tasks (emergency use only)
     */
    static void clearAllTasks();
    
    /**
     * @brief Get queue statistics
     * 
     * @param queueSize Output: current queue size
     * @param droppedTasks Output: number of dropped tasks
     */
    static void getStats(size_t& queueSize, unsigned long& droppedTasks);
    
    /**
     * @brief Print scheduler status for debugging
     */
    static void printStatus();

private:
    // Task queue (circular buffer)
    static Task taskQueue[MAX_TASK_QUEUE_SIZE];
    static size_t queueHead;
    static size_t queueTail;
    static size_t queueCount;
    
    // Current system state
    static SystemState currentState;
    static ScheduledTaskType currentTask;
    static unsigned long currentTaskStartTime;
    
    // Statistics
    static unsigned long droppedTaskCount;
    static unsigned long completedTaskCount;
    
    // Helper functions
    static TaskPriority getTaskPriority(ScheduledTaskType type);
    static SystemState getStateForTask(ScheduledTaskType type);
    static const char* getTaskName(ScheduledTaskType type);
    static const char* getStateName(SystemState state);
    static bool isQueueFull();
    static bool isQueueEmpty();
    static Task popHighestPriorityTask();
    
    // Prevent instantiation
    TaskScheduler() = delete;
};

#endif // TASK_SCHEDULER_H
