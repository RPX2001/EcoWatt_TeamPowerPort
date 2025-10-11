/**
 * @file Acquisition.cpp
 * @brief Implementation of the AcquisitionScheduler class for inverter data acquisition.
 *
 * @details
 * Implements the AcquisitionScheduler methods, providing an interface for polling
 * samples from an InverterSIM instance. Designed for integration with system coordinators
 * or other scheduling logic to facilitate periodic or event-driven data acquisition.
 *
 * @author Yasith
 * @author Prabath
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-08-18) Moved implementations to cpp file and split to layers.
 */


#include "Acquisition.h"

/**
 * @brief AcquisitionScheduler::AcquisitionScheduler
 * Construct an AcquisitionScheduler bound to a specific inverter simulator.
 *
 * @param sim [in] Reference to an InverterSIM instance to acquire samples from.
 *
 * @note The referenced InverterSIM instance must outlive this scheduler.
 */
AcquisitionScheduler::AcquisitionScheduler(InverterSIM& sim) : sim_(sim) {}

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
std::pair<bool, Sample> AcquisitionScheduler::poll_once() 
{ 
  return sim_.read(); 
}
