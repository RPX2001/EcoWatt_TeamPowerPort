#pragma once

#include <utility>
#include "sim/InverterSim.h"

/**
 * @class AcquisitionScheduler
 * @brief Schedules and executes inverter data acquisition operations.
 *
 * @details
 * Provides an interface to poll a single sample from an InverterSIM instance.
 * Designed for integration with Coordinators or other scheduling logic.
 */
class AcquisitionScheduler 
{
    public:
        /**
         * @brief AcquisitionScheduler::AcquisitionScheduler
         * Construct an AcquisitionScheduler bound to a specific inverter simulator.
         *
         * @param sim [in] Reference to an InverterSIM instance to acquire samples from.
         *
         * @note The referenced InverterSIM instance must outlive this scheduler.
         */
        explicit AcquisitionScheduler(InverterSIM& sim) : sim_(sim) {}

        /**
         * @brief AcquisitionScheduler::poll_once
         * Poll the inverter once to obtain a sample.
         *
         * @return std::pair<bool, Sample> 
         * - first: true if the sample was successfully obtained, false if acquisition failed.
         * - second: the acquired Sample if successful; otherwise default-initialized.
         *
         * @details Delegates the polling operation to the underlying InverterSIM instance.
         */
        std::pair<bool, Sample> poll_once() { return sim_.read(); }
    private:
        InverterSIM& sim_;
};
