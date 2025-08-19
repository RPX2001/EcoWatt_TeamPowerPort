#pragma once
#include <random>
#include <chrono>
#include <thread>
#include <cmath>

struct Sample {
    double t;
    double voltage;
    double current;
    double power;
};

class InverterSIM {
public:
    std::pair<bool, Sample> read() {
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
    static double now_epoch() {
        using Clock = std::chrono::system_clock;
        auto now = Clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }
    double rand01_() {
        static thread_local std::mt19937_64 rng{std::random_device{}()};
        static thread_local std::uniform_real_distribution<double> dist(0.0,1.0);
        return dist(rng);
    }
    double uniform_(double a,double b){
        static thread_local std::mt19937_64 rng{std::random_device{}()};
        std::uniform_real_distribution<double> dist(a,b);
        return dist(rng);
    }
};