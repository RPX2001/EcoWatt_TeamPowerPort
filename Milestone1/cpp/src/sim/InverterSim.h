#pragma once
#include <random>
#include <chrono>
#include <thread>

struct Sample {
    double t;        // epoch seconds
    double voltage;
    double current;
    double power;
};

class InverterSIM {
public:
    // returns (ok, sample)
    std::pair<bool, Sample> read() {
        // emulate IO latency ~50ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 2% transient failure
        if (rand01_() < 0.02) return {false, {}};

        double v = uniform_(210.0, 240.0);
        double i = uniform_(0.2, 2.0);
        Sample s{};
        s.t = now_epoch();
        s.voltage = round2(v);
        s.current = round3(i);
        s.power   = s.voltage * s.current;
        return {true, s};
    }

private:
    static double now_epoch() {
        using Clock = std::chrono::system_clock;
        auto now = Clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }
    static double round2(double x){ return std::round(x*100.0)/100.0; }
    static double round3(double x){ return std::round(x*1000.0)/1000.0; }

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