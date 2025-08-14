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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (rand01_() < 0.05) {
            std::cout << "Upload failed — re-queue for next window\n";
            return false;
        }
        // Prefer single ACK line from coordinator; return true silently.
        return true;
    }
private:
    double rand01_() {
        static thread_local std::mt19937_64 rng{std::random_device{}()};
        static thread_local std::uniform_real_distribution<double> dist(0.0,1.0);
        return dist(rng);
    }
};