/**
 * @file task_coordinator.cpp
 * @brief Implementation of task coordination and timer management
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/task_coordinator.h"
#include "peripheral/print.h"

// Initialize static members
hw_timer_t* TaskCoordinator::pollTimer = nullptr;
hw_timer_t* TaskCoordinator::uploadTimer = nullptr;
hw_timer_t* TaskCoordinator::changesTimer = nullptr;
hw_timer_t* TaskCoordinator::otaTimer = nullptr;

volatile bool TaskCoordinator::pollToken = false;
volatile bool TaskCoordinator::uploadToken = false;
volatile bool TaskCoordinator::changesToken = false;
volatile bool TaskCoordinator::otaToken = false;

uint64_t TaskCoordinator::currentPollFreq = 0;
uint64_t TaskCoordinator::currentUploadFreq = 0;
uint64_t TaskCoordinator::currentChangesFreq = 0;
uint64_t TaskCoordinator::currentOTAFreq = 0;

// ISR Handlers
void IRAM_ATTR TaskCoordinator::onPollTimer() {
    pollToken = true;
}

void IRAM_ATTR TaskCoordinator::onUploadTimer() {
    uploadToken = true;
}

void IRAM_ATTR TaskCoordinator::onChangesTimer() {
    changesToken = true;
}

void IRAM_ATTR TaskCoordinator::onOTATimer() {
    otaToken = true;
}

// Public Methods
bool TaskCoordinator::init(uint64_t pollFreqUs, uint64_t uploadFreqUs, 
                          uint64_t changesFreqUs, uint64_t otaFreqUs) {
    print("[TaskCoordinator] Initializing timers...\n");
    
    // Timer 0: Poll Timer
    pollTimer = timerBegin(0, 80, true); // Prescaler 80 (1us per tick)
    if (!pollTimer) {
        print("[TaskCoordinator] ERROR: Failed to initialize poll timer\n");
        return false;
    }
    timerAttachInterrupt(pollTimer, &onPollTimer, true);
    timerAlarmWrite(pollTimer, pollFreqUs, true);
    timerAlarmEnable(pollTimer);
    currentPollFreq = pollFreqUs;
    print("[TaskCoordinator] Poll timer: %llu us\n", pollFreqUs);
    
    // Timer 1: Upload Timer
    uploadTimer = timerBegin(1, 80, true);
    if (!uploadTimer) {
        print("[TaskCoordinator] ERROR: Failed to initialize upload timer\n");
        return false;
    }
    timerAttachInterrupt(uploadTimer, &onUploadTimer, true);
    timerAlarmWrite(uploadTimer, uploadFreqUs, true);
    timerAlarmEnable(uploadTimer);
    currentUploadFreq = uploadFreqUs;
    print("[TaskCoordinator] Upload timer: %llu us\n", uploadFreqUs);
    
    // Timer 2: Changes Check Timer
    changesTimer = timerBegin(2, 80, true);
    if (!changesTimer) {
        print("[TaskCoordinator] ERROR: Failed to initialize changes timer\n");
        return false;
    }
    timerAttachInterrupt(changesTimer, &onChangesTimer, true);
    timerAlarmWrite(changesTimer, changesFreqUs, true);
    timerAlarmEnable(changesTimer);
    currentChangesFreq = changesFreqUs;
    print("[TaskCoordinator] Changes timer: %llu us\n", changesFreqUs);
    
    // Timer 3: OTA Check Timer
    otaTimer = timerBegin(3, 80, true);
    if (!otaTimer) {
        print("[TaskCoordinator] ERROR: Failed to initialize OTA timer\n");
        return false;
    }
    timerAttachInterrupt(otaTimer, &onOTATimer, true);
    timerAlarmWrite(otaTimer, otaFreqUs, true);
    timerAlarmEnable(otaTimer);
    currentOTAFreq = otaFreqUs;
    print("[TaskCoordinator] OTA timer: %llu us\n", otaFreqUs);
    
    print("[TaskCoordinator] All timers initialized successfully\n");
    return true;
}

void TaskCoordinator::updatePollFrequency(uint64_t newFreqUs) {
    if (pollTimer && newFreqUs > 0) {
        timerAlarmWrite(pollTimer, newFreqUs, true);
        currentPollFreq = newFreqUs;
        print("[TaskCoordinator] Poll frequency updated: %llu us\n", newFreqUs);
    }
}

void TaskCoordinator::updateUploadFrequency(uint64_t newFreqUs) {
    if (uploadTimer && newFreqUs > 0) {
        timerAlarmWrite(uploadTimer, newFreqUs, true);
        currentUploadFreq = newFreqUs;
        print("[TaskCoordinator] Upload frequency updated: %llu us\n", newFreqUs);
    }
}

void TaskCoordinator::updateChangesFrequency(uint64_t newFreqUs) {
    if (changesTimer && newFreqUs > 0) {
        timerAlarmWrite(changesTimer, newFreqUs, true);
        currentChangesFreq = newFreqUs;
        print("[TaskCoordinator] Changes frequency updated: %llu us\n", newFreqUs);
    }
}

void TaskCoordinator::updateOTAFrequency(uint64_t newFreqUs) {
    if (otaTimer && newFreqUs > 0) {
        timerAlarmWrite(otaTimer, newFreqUs, true);
        currentOTAFreq = newFreqUs;
        print("[TaskCoordinator] OTA frequency updated: %llu us\n", newFreqUs);
    }
}

void TaskCoordinator::pauseAllTasks() {
    print("[TaskCoordinator] Pausing all tasks\n");
    if (pollTimer) timerAlarmDisable(pollTimer);
    if (uploadTimer) timerAlarmDisable(uploadTimer);
    if (changesTimer) timerAlarmDisable(changesTimer);
    if (otaTimer) timerAlarmDisable(otaTimer);
}

void TaskCoordinator::resumeAllTasks() {
    print("[TaskCoordinator] Resuming all tasks\n");
    if (pollTimer) timerAlarmEnable(pollTimer);
    if (uploadTimer) timerAlarmEnable(uploadTimer);
    if (changesTimer) timerAlarmEnable(changesTimer);
    if (otaTimer) timerAlarmEnable(otaTimer);
}

void TaskCoordinator::pauseTask(TaskType task) {
    switch (task) {
        case TASK_POLL:
            if (pollTimer) {
                timerAlarmDisable(pollTimer);
                print("[TaskCoordinator] Poll task paused\n");
            }
            break;
        case TASK_UPLOAD:
            if (uploadTimer) {
                timerAlarmDisable(uploadTimer);
                print("[TaskCoordinator] Upload task paused\n");
            }
            break;
        case TASK_CHANGES:
            if (changesTimer) {
                timerAlarmDisable(changesTimer);
                print("[TaskCoordinator] Changes task paused\n");
            }
            break;
        case TASK_OTA:
            if (otaTimer) {
                timerAlarmDisable(otaTimer);
                print("[TaskCoordinator] OTA task paused\n");
            }
            break;
    }
}

void TaskCoordinator::resumeTask(TaskType task) {
    switch (task) {
        case TASK_POLL:
            if (pollTimer) {
                timerAlarmEnable(pollTimer);
                print("[TaskCoordinator] Poll task resumed\n");
            }
            break;
        case TASK_UPLOAD:
            if (uploadTimer) {
                timerAlarmEnable(uploadTimer);
                print("[TaskCoordinator] Upload task resumed\n");
            }
            break;
        case TASK_CHANGES:
            if (changesTimer) {
                timerAlarmEnable(changesTimer);
                print("[TaskCoordinator] Changes task resumed\n");
            }
            break;
        case TASK_OTA:
            if (otaTimer) {
                timerAlarmEnable(otaTimer);
                print("[TaskCoordinator] OTA task resumed\n");
            }
            break;
    }
}

bool TaskCoordinator::isPollReady() {
    return pollToken;
}

bool TaskCoordinator::isUploadReady() {
    return uploadToken;
}

bool TaskCoordinator::isChangesReady() {
    return changesToken;
}

bool TaskCoordinator::isOTAReady() {
    return otaToken;
}

void TaskCoordinator::resetPollToken() {
    pollToken = false;
}

void TaskCoordinator::resetUploadToken() {
    uploadToken = false;
}

void TaskCoordinator::resetChangesToken() {
    changesToken = false;
}

void TaskCoordinator::resetOTAToken() {
    otaToken = false;
}

uint64_t TaskCoordinator::getPollFrequency() {
    return currentPollFreq;
}

uint64_t TaskCoordinator::getUploadFrequency() {
    return currentUploadFreq;
}

void TaskCoordinator::shutdown() {
    print("[TaskCoordinator] Shutting down all timers\n");
    
    if (pollTimer) {
        timerAlarmDisable(pollTimer);
        timerEnd(pollTimer);
        pollTimer = nullptr;
    }
    
    if (uploadTimer) {
        timerAlarmDisable(uploadTimer);
        timerEnd(uploadTimer);
        uploadTimer = nullptr;
    }
    
    if (changesTimer) {
        timerAlarmDisable(changesTimer);
        timerEnd(changesTimer);
        changesTimer = nullptr;
    }
    
    if (otaTimer) {
        timerAlarmDisable(otaTimer);
        timerEnd(otaTimer);
        otaTimer = nullptr;
    }
    
    print("[TaskCoordinator] Shutdown complete\n");
}
