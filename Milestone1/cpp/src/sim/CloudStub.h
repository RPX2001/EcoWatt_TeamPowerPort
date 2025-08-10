#pragma once
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <iostream>
#include "InverterSim.h"

class CloudStub {
public:
    bool upload(const std::vector<Sample>& batch) {
        // emulate network latency ~100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 5% NACK
        if (rand01_() < 0.05) {
            std::cout << "[CloudStub] NACK\n";
            return false;
        }
        std::cout << "[CloudStub] ACK | received " << batch.size() << " samples\n";
        return true;
    }

private:
    double rand01_() {
        static thread_local std::mt19937_64 rng{std::random_device{}()};
        static thread_local std::uniform_real_distribution<double> dist(0.0,1.0);
        return dist(rng);
    }
};