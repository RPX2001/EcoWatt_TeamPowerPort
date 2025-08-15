#include "Timers.h"

/**
 * @fn PeriodicTimer::PeriodicTimer
 * @brief Construct a periodic timer with a given period, name, and callback.
 *
 * @param period_seconds [in] The interval between ticks, in seconds.
 * @param name [in] A human-readable name for the timer (used for identification/logging).
 * @param on_tick [in] Function to call at each tick interval.
 *
 * @note The callback will be executed in the context of the timer's worker thread.
 */
PeriodicTimer::PeriodicTimer(double period_seconds, std::string name, TickFn on_tick)
    : period_(period_seconds), name_(std::move(name)), on_tick_(std::move(on_tick)) {}

/**
 * @fn PeriodicTimer::start
 * @brief Start the timer and begin executing the callback at fixed intervals.
 *
 * @details
 * - The timer starts immediately and waits for the first period before calling `on_tick_()`.
 * - Runs in a background thread until `stop()` is called or the object is destroyed.
 * - Thread-safe.
 */
void PeriodicTimer::start() 
{
    running_.store(true);
    worker_ = std::thread([this]
        {
            auto next = Clock::now() + Sec(period_);
            while (running_.load()) 
            {
                std::this_thread::sleep_until(next);
                if (!running_.load()) break;
                on_tick_();                 // Coordinator prints human messages
                next += Sec(period_);
            }
        });
}

/**
 * @fn PeriodicTimer::stop
 * @brief Stop the timer and wait for the worker thread to finish.
 *
 * @details
 * Safe to call multiple times; has no effect if the timer is not running.
 */
void PeriodicTimer::stop() 
{ 
        running_.store(false); 
        if (worker_.joinable()) worker_.join(); 
}

/**
 * @fn PeriodicTimer::~PeriodicTimer
 * @brief Destructor that ensures the timer is stopped before destruction.
 */
PeriodicTimer::~PeriodicTimer() 
{ 
    stop(); 
}

