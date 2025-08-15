#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

/**
 * @class PeriodicTimer
 * @brief Executes a callback function at a fixed periodic interval in a separate thread.
 *
 * @details
 * This class uses a steady clock to trigger a user-supplied callback (`on_tick_`)
 * at a fixed period specified in seconds. The timer runs in a background worker thread
 * and can be started or stopped explicitly.
 *
 * The timer stops automatically on destruction.
 */
class PeriodicTimer {
    public:
        using Clock = std::chrono::steady_clock;
        using Sec   = std::chrono::duration<double>;
        using TickFn = std::function<void()>;

        /**
         * @fn PeriodicTimer(double period_seconds, std::string name, TickFn on_tick)
         * @brief PeriodicTimer::PeriodicTimer
         * Construct a periodic timer with a given period, name, and callback.
         *
         * @param period_seconds [in] The interval between ticks, in seconds.
         * @param name [in] A human-readable name for the timer (used for identification/logging).
         * @param on_tick [in] Function to call at each tick interval.
         *
         * @note The callback will be executed in the context of the timer's worker thread.
         */
        PeriodicTimer(double period_seconds, std::string name, TickFn on_tick)
            : period_(period_seconds), name_(std::move(name)), on_tick_(std::move(on_tick)) {}

        /**
         * @fn start()
         * @brief PeriodicTimer::start
         * Start the timer and begin executing the callback at fixed intervals.
         *
         * @details
         * - The timer starts immediately and waits for the first period before calling `on_tick_()`.
         * - Runs in a background thread until `stop()` is called or the object is destroyed.
         * - Thread-safe.
         */
        void start() 
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
         * @fn stop()
         * @brief PeriodicTimer::stop
         * Stop the timer and wait for the worker thread to finish.
         *
         * @details
         * Safe to call multiple times; has no effect if the timer is not running.
         */
        void stop() 
        { 
                running_.store(false); 
                if (worker_.joinable()) worker_.join(); 
        }

        /**
         * @brief PeriodicTimer::~PeriodicTimer
         * Destructor that ensures the timer is stopped before destruction.
         */
        ~PeriodicTimer() 
        { 
            stop(); 
        }
    
    private:
        double period_;
        std::string name_;
        TickFn on_tick_;
        std::atomic<bool> running_{false};
        std::thread worker_;
};
