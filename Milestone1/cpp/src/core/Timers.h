#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

class PeriodicTimer {
public:
    using Clock = std::chrono::steady_clock;
    using Sec   = std::chrono::duration<double>;
    using TickFn = std::function<void()>;

    PeriodicTimer(double period_seconds, std::string name, TickFn on_tick)
        : period_(period_seconds), name_(std::move(name)), on_tick_(std::move(on_tick)) {}

    void start() {
        running_.store(true);
        worker_ = std::thread([this]{
            auto next = Clock::now() + Sec(period_);
            while (running_.load()) {
                std::this_thread::sleep_until(next);
                if (!running_.load()) break;
                on_tick_();                 // Coordinator prints human messages
                next += Sec(period_);
            }
        });
    }
    void stop() { running_.store(false); if (worker_.joinable()) worker_.join(); }
    ~PeriodicTimer() { stop(); }

private:
    double period_;
    std::string name_;
    TickFn on_tick_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};