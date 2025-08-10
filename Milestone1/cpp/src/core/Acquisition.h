#pragma once
#include <iostream>
#include "InverterSim.h"
#include "RingBuffer.h"

class AcquisitionScheduler {
public:
    AcquisitionScheduler(InverterSIM& sim, RingBuffer<Sample>& buffer)
        : sim_(sim), buffer_(buffer) {}

    void poll_once() {
        // T1_DoPoll {SIM_OK}
        auto [ok, s] = sim_.read();
        if (!ok) {
            std::cout << "[T1_DoPoll] SIM_OK=false -> poll skipped\n";
            return;
        }

        std::cout << "[P3 SampleReady] {"
                  << "'t': " << s.t << ", 'voltage': " << s.voltage
                  << ", 'current': " << s.current << ", 'power': " << s.power
                  << "}\n";

        // T2_BufferPush {BUF_HAS_SPACE}
        bool pushed = buffer_.push(s);
        if (pushed) {
            std::cout << "[T2_BufferPush] P4 size=" << buffer_.size() << "\n";
        }
    }

private:
    InverterSIM& sim_;
    RingBuffer<Sample>& buffer_;
};