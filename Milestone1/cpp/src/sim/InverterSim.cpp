/**
 * @file InverterSim.cpp
 * @brief Implementation of the InverterSIM class for simulating inverter measurements.
 *
 * @details
 * Implements the InverterSIM methods, generating pseudo-random voltage, current, and power
 * readings to simulate an inverter. Introduces acquisition delays and occasional read failures
 * to mimic real hardware behavior for testing and development.
 *
 * @author Yasith
 * @author Prabath
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-08-18) Moved implementations to cpp file.
 */


#include "sim/InverterSim.h"

/**
 * @fn InverterSIM::read
 * @brief Acquire a new simulated inverter reading.
 *
 * @return std::pair<bool, Sample>
 * - `first` = true if reading was successful, false if acquisition failed.
 * - `second` = Valid Sample if successful, otherwise default-initialized.
 *
 * @details
 * - Introduces a blocking delay of ~50 ms to simulate sensor read time.
 * - Has a 2% probability of simulated failure.
 * - Voltage range: 210.0–240.0 V (rounded to 2 decimal places).
 * - Current range: 0.2–2.0 A (rounded to 3 decimal places).
 * - Power calculated as voltage × current.
 *
 * @note Caller must handle the failure case (`first == false`).
 */
std::pair<bool, Sample> InverterSIM::read() 
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (rand01_() < 0.02) return {false, {}};

    double v = uniform_(210.0, 240.0);
    double i = uniform_(0.2, 2.0);
    Sample s{};
    s.t = now_epoch();
    s.voltage = std::round(v * 100.0) / 100.0;
    s.current = std::round(i * 1000.0) / 1000.0;
    s.power   = s.voltage * s.current;
    return {true, s};
}

/**
 * @fn InverterSIM::now_epoch
 * @brief Get the current epoch time in seconds.
 *
 * @return double Current time since the Unix epoch in seconds.
 */
double InverterSIM::now_epoch() 
{
    using Clock = std::chrono::system_clock;
    auto now = Clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

/**
 * @fn InverterSIM::rand01_
 * @brief Generate a random double between 0.0 and 1.0.
 *
 * @return double Random value in the range [0.0, 1.0].
 *
 * @details Uses a thread-local Mersenne Twister engine.
 */
double InverterSIM::rand01_() 
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static thread_local std::uniform_real_distribution<double> dist(0.0,1.0);
    return dist(rng);
}

/**
 * @fn InverterSIM::uniform_
 * @brief Generate a random double between a specified range.
 *
 * @param a [in] Lower bound of the range.
 * @param b [in] Upper bound of the range.
 * @return double Random value in the range [a, b].
 *
 * @details Uses a thread-local Mersenne Twister engine.
 */
double InverterSIM::uniform_(double a,double b)
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(a,b);
    return dist(rng);
}
