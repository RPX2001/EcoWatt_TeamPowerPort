#pragma once

#include <random>
#include <chrono>
#include <thread>
#include <cmath>

/**
 * @struct Sample
 * @brief Represents a single measurement from the inverter.
 *
 * @var Sample::t
 * Epoch timestamp (seconds since 1970-01-01 00:00:00 UTC).
 * @var Sample::voltage
 * Voltage reading in volts (rounded to two decimal places).
 * @var Sample::current
 * Current reading in amperes (rounded to three decimal places).
 * @var Sample::power
 * Power reading in watts (calculated as voltage × current).
 */
struct Sample 
{
    double t;
    double voltage;
    double current;
    double power;
};

/**
 * @class InverterSIM
 * @brief Simulates an inverter that produces electrical readings.
 *
 * @details
 * Generates pseudo-random voltage, current, and power measurements.
 * Adds simulated acquisition delay and occasional read failures
 * to mimic real hardware behaviour.
 */
class InverterSIM 
{
    public:
        /**
         * @brief InverterSIM::read
         * Acquire a new simulated inverter reading.
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
        std::pair<bool, Sample> read() 
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

    private:
        /**
         * @brief InverterSIM::now_epoch
         * Get the current epoch time in seconds.
         *
         * @return double Current time since the Unix epoch in seconds.
         */
        static double now_epoch() 
        {
            using Clock = std::chrono::system_clock;
            auto now = Clock::now().time_since_epoch();
            return std::chrono::duration<double>(now).count();
        }

        /**
         * @brief InverterSIM::rand01_
         * Generate a random double between 0.0 and 1.0.
         *
         * @return double Random value in the range [0.0, 1.0].
         *
         * @details Uses a thread-local Mersenne Twister engine.
         */
        double rand01_() 
        {
            static thread_local std::mt19937_64 rng{std::random_device{}()};
            static thread_local std::uniform_real_distribution<double> dist(0.0,1.0);
            return dist(rng);
        }

        /**
         * @brief InverterSIM::uniform_
         * Generate a random double between a specified range.
         *
         * @param a [in] Lower bound of the range.
         * @param b [in] Upper bound of the range.
         * @return double Random value in the range [a, b].
         *
         * @details Uses a thread-local Mersenne Twister engine.
         */
        double uniform_(double a,double b)
        {
            static thread_local std::mt19937_64 rng{std::random_device{}()};
            std::uniform_real_distribution<double> dist(a,b);
            return dist(rng);
        }
};
