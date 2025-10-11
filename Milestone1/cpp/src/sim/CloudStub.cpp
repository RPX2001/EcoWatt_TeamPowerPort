/**
 * @file CloudStub.cpp
 * @brief Implementation of the CloudStub class for simulating cloud uploads of inverter samples.
 *
 * @details
 * Implements the CloudStub methods, simulating a cloud service for uploading batches of inverter
 * samples. Introduces artificial delays and a configurable failure probability to mimic network
 * unreliability during testing and development.
 *
 * @author Yasith
 * @author Prabath
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-08-18) Moved implementations to cpp file.
 */


#include "sim/CloudStub.h"

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
bool CloudStub::upload(const std::vector<Sample>& batch)
{
    (void)batch;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (rand01_() < 0.05)
    {
        std::cout << "Upload failed — re-queue for next window\n";
        return false;
    }
    // Prefer single ACK line from coordinator; return true silently.
    return true;
}

/**
  * @fn rand01_()
  * @brief Generates a random double between 0.0 and 1.0.
  *
  * Uses a thread-local Mersenne Twister engine for generating
  * uniformly distributed random numbers.
  *
  * @return A random number in the range [0.0, 1.0].
  */
double CloudStub::rand01_() 
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static thread_local std::uniform_real_distribution<double> dist(0.0,1.0);
    return dist(rng);
}
