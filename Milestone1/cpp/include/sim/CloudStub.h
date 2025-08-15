#pragma once

#include "InverterSim.h"

#include <vector>

/**
 * @class CloudStub
 * @brief Simulates a cloud service for uploading batches of inverter samples.
 *
 * This class acts as a stub or mock for a real cloud upload mechanism.
 * It introduces artificial delays and a configurable failure probability
 * to mimic network unreliability during testing.
 */
class CloudStub 
{
    public:
        /**
         * @fn upload(const std::vector<Sample>& batch) 
         * @brief Uploads a batch of samples to the simulated cloud service.
         *
         * The function sleeps for 100 ms to simulate network latency.
         * There is a 5% random chance that the upload will fail, in which case
         * the caller should re-queue the batch for retry in the next window.
         *
         * @param   batch A vector containing the samples to upload.
         * @return `true` if the upload was successful, `false` if it failed.
         */
        bool upload(const std::vector<Sample>& batch);
    
    private:
         /**
          * @fn rand01_()
          * @brief Generates a random double between 0.0 and 1.0.
          *
          * Uses a thread-local Mersenne Twister engine for generating
          * uniformly distributed random numbers.
          *
          * @return A random number in the range [0.0, 1.0].
          */
        double rand01_();
};
