#pragma once
#include <utility>
#include "sim/InverterSim.h"

class AcquisitionScheduler {
public:
    explicit AcquisitionScheduler(InverterSIM& sim) : sim_(sim) {}
    std::pair<bool, Sample> poll_once() { return sim_.read(); }
private:
    InverterSIM& sim_;
};