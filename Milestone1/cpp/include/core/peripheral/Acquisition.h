/**
 * @file Acquisition.h
 * @brief Declaration of the AcquisitionScheduler class for inverter data acquisition.
 *
 * @details
 * Defines the AcquisitionScheduler class, which provides an interface for polling
 * samples from an InverterSIM instance. Designed for integration with system coordinators
 * or other scheduling logic to facilitate periodic or event-driven data acquisition.
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

#include <utility>
#include "ProtocolAdapter.h"
#include "Debug.h"

//Save the decoded register values in this struct
struct DecodedValues {
  uint16_t values[10];   // holds decoded register values
  size_t count;          // number of valid values
};

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
        explicit AcquisitionScheduler(InverterSIM& sim);

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
        std::pair<bool, DecodedValues> poll_once(const RegID* regs, size_t regCount);

    private:
        InverterSIM& sim_;
        DecodedValues decodeReadResponse(const String& frameHex, uint16_t startAddr, uint16_t count, const RegID* regs, size_t regCount);
};
