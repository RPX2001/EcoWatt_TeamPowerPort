/**
 * @file Coordinator.cpp
 * @brief Implementation of the Coordinator class for managing acquisition and upload events.
 *
 * @details
 * Implements the Coordinator class methods, handling the main event-processing loop,
 * state transitions, polling, and uploading of inverter samples. Coordinates between
 * the event queue, ring buffer, acquisition scheduler, and uploader to manage system
 * state and ensure reliable operation.
 *
 * @author Yasith
 * @author Prabath
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-08-18) Moved implementations to cpp file and split to layers.
 */


#include "Coordinator.h"

/**
 * @fn Coordinator::Coordinator
 * @brief Construct a Coordinator with required references to system components.
 *
 * @param q [in] Reference to the event queue.
 * @param buffer [in] Reference to the ring buffer storing samples.
 * @param acq [in] Reference to the acquisition scheduler for polling samples.
 * @param upl [in] Reference to the uploader for sending samples.
 * @param running [in] Reference to a shared atomic flag controlling execution.
 *
 * @note All referenced objects must outlive the Coordinator instance.
 */
Coordinator::Coordinator(BlockingQueue<Event>& q,
            RingBuffer<Sample>& buffer,
            AcquisitionScheduler& acq,
            Uploader& upl,
            std::atomic<bool>& running)
    : q_(q), buffer_(buffer), acq_(acq), upl_(upl), running_(running) {}

/**
 * @fn Coordinator::run
 * @brief Main event-processing loop for the Coordinator.
 *
 * @details
 * - Initializes internal state.
 * - Processes events from the queue until a shutdown condition is met.
 * - Responds to:
 *   - PollTick: Marks polling ready.
 *   - UploadTick: Marks uploading ready.
 *   - Shutdown: Terminates loop.
 */
void Coordinator::run() 
{
    idle_ = true; polling_ = false; uploading_ = false;
    poll_ready_ = false; upload_ready_ = false;

    for (;;) 
    {
        if (!running_.load()) break;
        Event ev = q_.pop();
        if (ev.kind == EventKind::Shutdown) break;

        if (ev.kind == EventKind::PollTick) 
        {
            poll_ready_ = true;
            std::cout << "[Poll Timer = 2s] tick -> Poll Ready\n";
        } 
        else if (ev.kind == EventKind::UploadTick) 
        {
            upload_ready_ = true;
            std::cout << "[Upload Timer = 15s] tick -> Upload Ready\n";
        }
        
        drain_enabled_transitions();
    }
}

/**
 * @fn Coordinator::request_stop
 * @brief Request the Coordinator to stop processing events.
 *
 * @details
 * Sets the shared `running_` flag to false, which will cause the main loop to exit.
 */
void Coordinator::request_stop()
{ 
    running_.store(false); 
}

/**
 * @fn Coordinator::drain_enabled_transitions
 * @brief Execute state transitions that are currently enabled.
 *
 * @details
 * - Checks for conditions to start uploading or polling.
 * - Continues looping while progress is made in state transitions.
 */
void Coordinator::drain_enabled_transitions() 
{
    bool progress = true;
    while (progress) 
    {
        progress = false;

        if (upload_ready_ && !polling_ && !uploading_ && buffer_.not_empty()) 
        {
            std::cout << "Not Polling -> Uploading\n";
            do_uploading();
            progress = true;
            continue;
        }
        
        if (poll_ready_ && !uploading_ && !polling_) 
        {
            std::cout << "Not Uploading -> Polling\n";
            do_polling();
            progress = true;
            continue;
        }
    }
}

/**
 * @fn Coordinator::do_polling
 * @brief Perform a polling operation to acquire a sample.
 *
 * @details
 * - Calls AcquisitionScheduler::poll_once to get a sample.
 * - If successful, stores the sample in the buffer.
 * - Handles buffer full condition.
 * - Updates internal state flags.
 */
void Coordinator::do_polling() 
{
    idle_ = false; polling_ = true;

    auto [ok, s] = acq_.poll_once();
    if (!ok) 
    {
        std::cout << "Poll skipped (device unavailable)\n";
        polling_ = false; idle_ = true; return;
    }

    std::cout << "Sample Ready {"
              << "'t': " << s.t << ", 'voltage': " << s.voltage
              << ", 'current': " << s.current << ", 'power': " << s.power
              << "}\n";

    bool pushed = buffer_.push(s);
    if (pushed) 
    {
        std::cout << "Buffer Push\n";
        std::cout << "Pushed | Buffer size = " << buffer_.size() << "\n";
    } 
    else 
    {
        std::cout << "Buffer full — sample dropped\n";
    }

    poll_ready_ = false; polling_ = false; idle_ = true;
    std::cout << "Idle started\n";
}

/**
 * @fn Coordinator::do_uploading
 * @brief Perform an upload operation for all buffered samples.
 *
 * @details
 * - Drains all samples from the buffer.
 * - Attempts to upload via Uploader::upload_once.
 * - On failure, requeues all samples back into the buffer.
 */
void Coordinator::do_uploading() 
{
    idle_ = false; uploading_ = true;

    auto batch = buffer_.drain_all();
    std::cout << "Uploading (sending " << batch.size() << " samples)\n";

    bool ok = upl_.upload_once(batch);
    if (ok) 
    {
        std::cout << "Received ACK -> Idle\n";
        upload_ready_ = false;
    } 
    else 
    {
        // CloudStub already printed failure; keep behavior consistent:
        for (const auto& s : batch) buffer_.push(s);
        upload_ready_ = false;
    }

    uploading_ = false; idle_ = true;
}
