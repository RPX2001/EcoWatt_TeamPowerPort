/**
 * @file task_scheduler.cpp
 * @brief Implementation of Real-Time Task Scheduler
 * 
 * @author Team PowerPort
 * @date 2025-10-28
 */

#include "application/task_scheduler.h"
#include "peripheral/print.h"

// Initialize static members
Task TaskScheduler::taskQueue[MAX_TASK_QUEUE_SIZE];
size_t TaskScheduler::queueHead = 0;
size_t TaskScheduler::queueTail = 0;
size_t TaskScheduler::queueCount = 0;
SystemState TaskScheduler::currentState = STATE_IDLE;
ScheduledTaskType TaskScheduler::currentTask = SCHED_TASK_NONE;
unsigned long TaskScheduler::currentTaskStartTime = 0;
unsigned long TaskScheduler::droppedTaskCount = 0;
unsigned long TaskScheduler::completedTaskCount = 0;

void TaskScheduler::init() {
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
    currentState = STATE_IDLE;
    currentTask = SCHED_TASK_NONE;
    currentTaskStartTime = 0;
    droppedTaskCount = 0;
    completedTaskCount = 0;
    
    print("[TaskScheduler] Initialized - Real-Time Priority Queue\n");
    print("[TaskScheduler] Max queue size: %d\n", MAX_TASK_QUEUE_SIZE);
}

bool TaskScheduler::queueTask(ScheduledTaskType type, TaskPriority priority) {
    // Don't queue if already queued (prevents duplicates)
    if (isTaskQueued(type)) {
        return true;  // Already queued, not an error
    }
    
    // Check if queue is full
    if (isQueueFull()) {
        print("[TaskScheduler] WARNING: Queue full, dropping task: %s\n", getTaskName(type));
        droppedTaskCount++;
        return false;
    }
    
    // Add task to queue
    taskQueue[queueTail] = Task(type, priority);
    queueTail = (queueTail + 1) % MAX_TASK_QUEUE_SIZE;
    queueCount++;
    
    print("[TaskScheduler] Queued: %s (Priority: %d, Queue: %zu/%d)\n", 
          getTaskName(type), priority, queueCount, MAX_TASK_QUEUE_SIZE);
    
    return true;
}

Task TaskScheduler::getNextTask() {
    // If FOTA is running, block ALL other tasks
    if (currentState == STATE_FOTA) {
        return Task(SCHED_TASK_NONE, PRIORITY_LOW);
    }
    
    // If any task is running, don't start another
    if (currentState != STATE_IDLE) {
        return Task(SCHED_TASK_NONE, PRIORITY_LOW);
    }
    
    // Queue empty
    if (isQueueEmpty()) {
        return Task(SCHED_TASK_NONE, PRIORITY_LOW);
    }
    
    // Get highest priority task
    return popHighestPriorityTask();
}

void TaskScheduler::taskStarted(ScheduledTaskType type) {
    currentTask = type;
    currentState = getStateForTask(type);
    currentTaskStartTime = millis();
    
    print("[TaskScheduler] Started: %s (State: %s)\n", 
          getTaskName(type), getStateName(currentState));
}

void TaskScheduler::taskCompleted() {
    unsigned long duration = millis() - currentTaskStartTime;
    
    print("[TaskScheduler] Completed: %s (Duration: %lu ms)\n", 
          getTaskName(currentTask), duration);
    
    currentTask = SCHED_TASK_NONE;
    currentState = STATE_IDLE;
    currentTaskStartTime = 0;
    completedTaskCount++;
}

bool TaskScheduler::isTaskQueued(ScheduledTaskType type) {
    if (isQueueEmpty()) return false;
    
    size_t index = queueHead;
    for (size_t i = 0; i < queueCount; i++) {
        if (taskQueue[index].type == type) {
            return true;
        }
        index = (index + 1) % MAX_TASK_QUEUE_SIZE;
    }
    
    return false;
}

bool TaskScheduler::isBusy() {
    return currentState != STATE_IDLE;
}

SystemState TaskScheduler::getCurrentState() {
    return currentState;
}

bool TaskScheduler::canStartFOTA() {
    // FOTA can only start if:
    // 1. System is IDLE
    // 2. No CRITICAL tasks in queue (Poll, Upload)
    
    if (currentState != STATE_IDLE) {
        return false;
    }
    
    // Check queue for CRITICAL priority tasks
    size_t index = queueHead;
    for (size_t i = 0; i < queueCount; i++) {
        if (taskQueue[index].priority == PRIORITY_CRITICAL) {
            return false;  // Critical task waiting
        }
        index = (index + 1) % MAX_TASK_QUEUE_SIZE;
    }
    
    return true;
}

void TaskScheduler::clearAllTasks() {
    print("[TaskScheduler] WARNING: Clearing all tasks!\n");
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
    currentTask = SCHED_TASK_NONE;
    currentState = STATE_IDLE;
}

void TaskScheduler::getStats(size_t& queueSize, unsigned long& droppedTasks) {
    queueSize = queueCount;
    droppedTasks = droppedTaskCount;
}

void TaskScheduler::printStatus() {
    print("\n========== TASK SCHEDULER STATUS ==========\n");
    print("  Current State:     %s\n", getStateName(currentState));
    print("  Current Task:      %s\n", getTaskName(currentTask));
    print("  Queue Size:        %zu/%d\n", queueCount, MAX_TASK_QUEUE_SIZE);
    print("  Completed Tasks:   %lu\n", completedTaskCount);
    print("  Dropped Tasks:     %lu\n", droppedTaskCount);
    
    if (currentState != STATE_IDLE) {
        unsigned long elapsed = millis() - currentTaskStartTime;
        print("  Task Running Time: %lu ms\n", elapsed);
    }
    
    if (queueCount > 0) {
        print("  Queued Tasks:\n");
        size_t index = queueHead;
        for (size_t i = 0; i < queueCount; i++) {
            Task& task = taskQueue[index];
            unsigned long waitTime = millis() - task.queuedTime;
            print("    [%zu] %s (Priority: %d, Waiting: %lu ms)\n", 
                  i + 1, getTaskName(task.type), task.priority, waitTime);
            index = (index + 1) % MAX_TASK_QUEUE_SIZE;
        }
    }
    
    print("===========================================\n\n");
}

// Private helper functions

TaskPriority TaskScheduler::getTaskPriority(ScheduledTaskType type) {
    switch (type) {
        case SCHED_TASK_POLL_SENSORS:
        case SCHED_TASK_UPLOAD_DATA:
            return PRIORITY_CRITICAL;
        case SCHED_TASK_CHECK_COMMANDS:
            return PRIORITY_HIGH;
        case SCHED_TASK_CHECK_CONFIG:
            return PRIORITY_MEDIUM;
        case SCHED_TASK_CHECK_FOTA:
            return PRIORITY_LOW;
        default:
            return PRIORITY_MEDIUM;
    }
}

SystemState TaskScheduler::getStateForTask(ScheduledTaskType type) {
    switch (type) {
        case SCHED_TASK_POLL_SENSORS:   return STATE_POLLING;
        case SCHED_TASK_UPLOAD_DATA:    return STATE_UPLOADING;
        case SCHED_TASK_CHECK_COMMANDS: return STATE_COMMANDING;
        case SCHED_TASK_CHECK_CONFIG:   return STATE_CONFIG_CHECK;
        case SCHED_TASK_CHECK_FOTA:     return STATE_FOTA;
        default:                  return STATE_IDLE;
    }
}

const char* TaskScheduler::getTaskName(ScheduledTaskType type) {
    switch (type) {
        case SCHED_TASK_POLL_SENSORS:   return "POLL_SENSORS";
        case SCHED_TASK_UPLOAD_DATA:    return "UPLOAD_DATA";
        case SCHED_TASK_CHECK_COMMANDS: return "CHECK_COMMANDS";
        case SCHED_TASK_CHECK_CONFIG:   return "CHECK_CONFIG";
        case SCHED_TASK_CHECK_FOTA:     return "CHECK_FOTA";
        default:                  return "NONE";
    }
}

const char* TaskScheduler::getStateName(SystemState state) {
    switch (state) {
        case STATE_IDLE:         return "IDLE";
        case STATE_POLLING:      return "POLLING";
        case STATE_UPLOADING:    return "UPLOADING";
        case STATE_COMMANDING:   return "COMMANDING";
        case STATE_CONFIG_CHECK: return "CONFIG_CHECK";
        case STATE_FOTA:         return "FOTA";
        default:                 return "UNKNOWN";
    }
}

bool TaskScheduler::isQueueFull() {
    return queueCount >= MAX_TASK_QUEUE_SIZE;
}

bool TaskScheduler::isQueueEmpty() {
    return queueCount == 0;
}

Task TaskScheduler::popHighestPriorityTask() {
    if (isQueueEmpty()) {
        return Task(SCHED_TASK_NONE, PRIORITY_LOW);
    }
    
    // Find highest priority task
    size_t bestIndex = queueHead;
    TaskPriority bestPriority = taskQueue[queueHead].priority;
    
    size_t index = queueHead;
    for (size_t i = 0; i < queueCount; i++) {
        if (taskQueue[index].priority < bestPriority) {  // Lower number = higher priority
            bestPriority = taskQueue[index].priority;
            bestIndex = index;
        }
        index = (index + 1) % MAX_TASK_QUEUE_SIZE;
    }
    
    // Get the best task
    Task bestTask = taskQueue[bestIndex];
    
    // Remove it from queue by shifting elements
    if (bestIndex == queueHead) {
        // Task is at head, easy removal
        queueHead = (queueHead + 1) % MAX_TASK_QUEUE_SIZE;
    } else {
        // Task is in middle/end, shift elements
        size_t current = bestIndex;
        size_t next = (bestIndex + 1) % MAX_TASK_QUEUE_SIZE;
        
        while (next != queueTail) {
            taskQueue[current] = taskQueue[next];
            current = next;
            next = (next + 1) % MAX_TASK_QUEUE_SIZE;
        }
        
        queueTail = current;
    }
    
    queueCount--;
    
    return bestTask;
}
