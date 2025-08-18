/**
 * @file Coordinator.h
 * @brief Declaration of the Coordinator class for managing acquisition and upload events.
 *
 * @details
 * Defines the Coordinator class, event types, and supporting structures for orchestrating
 * polling and uploading of inverter samples. The Coordinator interacts with a blocking queue,
 * ring buffer, acquisition scheduler, and uploader to manage system state transitions.
 *
 * @author Prabath (original)
 * @author Yasith (revision)
 * @version 1.1
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Prabath, 2025-08-14) Original file.
 * - 1.1 (Yasith, 2025-08-18) Split into header and source files and split to layers.
 */


#pragma once

#include <atomic>
#include <iostream>
#include <vector>
#include "BlockingQueue.h"
#include "RingBuffer.h"
#include "Acquisition.h"
#include "Uploader.h"

/**
 * @enum EventKind
 * @brief Types of events processed by the Coordinator.
 */
enum class EventKind { PollTick, UploadTick, Shutdown };

/**
 * @struct Event
 * @brief Encapsulates a single event for the Coordinator.
 *
 * @var Event::kind
 * The type of event.
 */
struct Event { EventKind kind; };

/**
 * @class Coordinator
 * @brief Manages polling and uploading of inverter samples based on scheduled events.
 *
 * @details
 * The Coordinator listens for timer events from a BlockingQueue and transitions
 * between idle, polling, and uploading states accordingly. It interacts with:
 * - An AcquisitionScheduler to obtain inverter samples.
 * - A RingBuffer to store pending samples.
 * - A Uploader to send samples to the cloud.
 *
 * The Coordinator runs in its own loop until a Shutdown event or stop request is received.
 */
class Coordinator 
{
    public:
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
        Coordinator(BlockingQueue<Event>& q,
                    RingBuffer<Sample>& buffer,
                    AcquisitionScheduler& acq,
                    Uploader& upl,
                    std::atomic<bool>& running);

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
        void run();

        /**
         * @fn Coordinator::request_stop
         * @brief Request the Coordinator to stop processing events.
         *
         * @details
         * Sets the shared `running_` flag to false, which will cause the main loop to exit.
         */
        void request_stop();
    
    private:
        /**
         * @fn Coordinator::drain_enabled_transitions
         * @brief Execute state transitions that are currently enabled.
         *
         * @details
         * - Checks for conditions to start uploading or polling.
         * - Continues looping while progress is made in state transitions.
         */
        void drain_enabled_transitions();

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
        void do_polling();

        /**
         * @fn Coordinator::do_uploading
         * @brief Perform an upload operation for all buffered samples.
         *
         * @details
         * - Drains all samples from the buffer.
         * - Attempts to upload via Uploader::upload_once.
         * - On failure, requeues all samples back into the buffer.
         */
        void do_uploading();
    
    private:
        BlockingQueue<Event>& q_;
        RingBuffer<Sample>& buffer_;
        AcquisitionScheduler& acq_;
        Uploader& upl_;
        std::atomic<bool>& running_;
    
        bool idle_{true}, polling_{false}, uploading_{false};
        bool poll_ready_{false}, upload_ready_{false};
};
