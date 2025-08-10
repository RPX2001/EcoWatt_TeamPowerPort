#pragma once
#include <iostream>
#include <vector>
#include "RingBuffer.h"
#include "CloudStub.h"

class Uploader {
public:
    Uploader(RingBuffer<Sample>& buffer, CloudStub& cloud)
        : buffer_(buffer), cloud_(cloud) {}

    void upload_once() {
        if (!buffer_.not_empty()) {
            std::cout << "[Guard BUF_NOT_EMPTY] Buffer empty -> no upload\n";
            return;
        }

        // T4_UploadBatch
        auto batch = buffer_.drain_all();
        std::cout << "[T4_UploadBatch] sending batch of " << batch.size() << " samples\n";
        bool ok = cloud_.upload(batch);

        if (ok) {
            std::cout << "[T5_AckAndFlush] ACK received -> P7 Acked\n";
        } else {
            std::cout << "[Upload Failed] Re-queueing samples for next window\n";
            // Re-queue samples (simple strategy)
            for (const auto& s : batch) buffer_.push(s);
        }
    }

private:
    RingBuffer<Sample>& buffer_;
    CloudStub& cloud_;
};