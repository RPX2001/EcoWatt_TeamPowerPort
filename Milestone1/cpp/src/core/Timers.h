#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <iostream>

class PeriodicTimer {
public:
    using Clock = std::chrono::steady_clock;
    using Sec   = std::chrono::duration<double>;

    PeriodicTimer(double period_seconds, std::string name, std::function<void()> on_tick)
        : period_(period_seconds), name_(std::move(name)), on_tick_(std::move(on_tick)) {}

    void start() {
        running_.store(true);
        worker_ = std::thread([this] {
            log("armed");
            auto next = Clock::now() + to_duration(period_);
            while (running_.load()) {
                std::this_thread::sleep_until(next);
                if (!running_.load()) break;
                log("Ttick_" + name_ + " fired");
                on_tick_();
                next += to_duration(period_);
            }
        });
    }

    void stop() {
        running_.store(false);
        if (worker_.joinable()) worker_.join();
        log("stopped");
    }

    ~PeriodicTimer() { stop(); }

private:
    static Sec to_duration(double s) { return Sec(s); }
    void log(const std::string& msg) const { std::cout << "[" << name_ << " Timer] " << msg << "\n"; }

    double period_;
    std::string name_;
    std::function<void()> on_tick_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};