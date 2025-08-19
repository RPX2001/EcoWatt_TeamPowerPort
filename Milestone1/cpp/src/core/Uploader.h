#pragma once
#include <vector>
#include "sim/CloudStub.h"
#include "sim/InverterSim.h"

class Uploader {
public:
    explicit Uploader(CloudStub& cloud) : cloud_(cloud) {}
    bool upload_once(const std::vector<Sample>& batch) { return cloud_.upload(batch); }
private:
    CloudStub& cloud_;
};