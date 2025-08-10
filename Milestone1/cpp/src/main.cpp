#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include "core/Timers.h"
#include "core/RingBuffer.h"
#include "core/Acquisition.h"
#include "core/Uploader.h"
#include "sim/InverterSim.h"
#include "sim/CloudStub.h"

static std::atomic<bool> g_running{true};

void handle_sigint(int){ g_running.store(false); }

int main() {
    std::signal(SIGINT, handle_sigint);
    constexpr double POLL_PERIOD_S   = 2.0;   // fast sampling
    constexpr double UPLOAD_PERIOD_S = 15.0;  // 15s sim window
    constexpr size_t BUFFER_CAPACITY = 256;

    RingBuffer<Sample> buffer(BUFFER_CAPACITY);
    InverterSIM sim;
    CloudStub cloud;
    AcquisitionScheduler acq(sim, buffer);
    Uploader uploader(buffer, cloud);

    std::cout << "M0: P0=1, P8=1, P9=1 | Starting scaffold…\n";

    PeriodicTimer pollTimer(POLL_PERIOD_S, "Poll", [&]{
        if (!g_running.load()) return;
        acq.poll_once(); // T1_DoPoll -> P3 -> T2_BufferPush
    });

    PeriodicTimer uploadTimer(UPLOAD_PERIOD_S, "Upload", [&]{
        if (!g_running.load()) return;
        uploader.upload_once(); // T4_UploadBatch -> T5_AckAndFlush
    });

    pollTimer.start();
    uploadTimer.start();

    // Simple wait loop until Ctrl+C
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    pollTimer.stop();
    uploadTimer.stop();
    std::cout << "Stopped.\n";
    return 0;
}