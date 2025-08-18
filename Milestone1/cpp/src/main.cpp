/**
 * @file main.cpp
 * @brief Application entry point for inverter data acquisition and upload system.
 *
 * @details
 * Initializes system components, sets up periodic polling and upload timers,
 * and starts the main coordinator event loop. Handles clean shutdown on SIGINT.
 *
 * @author Prabath (original)
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Prabath, 2025-08-18) Original file.
 */


#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <iostream>
#include "core/peripheral/BlockingQueue.h"
#include "core/peripheral/Timers.h"
#include "core/peripheral/RingBuffer.h"
#include "core/peripheral/Acquisition.h"
#include "core/peripheral/Uploader.h"
#include "core/application/Coordinator.h"
#include "sim/InverterSim.h"
#include "sim/CloudStub.h"

/**
 * @brief Global atomic flag controlling the running state of the application.
 */
static std::atomic<bool> g_running{true};

/**
 * @fn handle_sigint
 * @brief Signal handler for SIGINT (Ctrl+C) to request application shutdown.
 *
 * @param sig [in] Signal number (unused).
 *
 * @details Sets the global `g_running` flag to false to initiate clean shutdown.
 */
void handle_sigint(int) 
{ 
    g_running.store(false);
}

int main() {
    std::signal(SIGINT, handle_sigint);

    constexpr double POLL_PERIOD_S   = 2.0;
    constexpr double UPLOAD_PERIOD_S = 15.0;
    constexpr size_t BUFFER_CAPACITY = 256;

    RingBuffer<Sample> buffer(BUFFER_CAPACITY);
    InverterSIM sim;
    CloudStub cloud;
    AcquisitionScheduler acq(sim);
    Uploader upl(cloud);

    BlockingQueue<Event> q;
    Coordinator coord(q, buffer, acq, upl, g_running);

    std::cout << "Idle started | (Re)start Poll Timer | (Re)start Upload Timer\n";

    PeriodicTimer pollTimer(POLL_PERIOD_S, "Poll", [&]{ q.push({EventKind::PollTick}); });
    PeriodicTimer uploadTimer(UPLOAD_PERIOD_S, "Upload", [&]{ q.push({EventKind::UploadTick}); });

    pollTimer.start();
    uploadTimer.start();

    std::thread coordThread([&]{ coord.run(); });

    while (g_running.load()) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    pollTimer.stop();
    uploadTimer.stop();
    q.push({EventKind::Shutdown});
    coord.request_stop();
    if (coordThread.joinable()) coordThread.join();

    std::cout << "Stopped.\n";
    return 0;
}
