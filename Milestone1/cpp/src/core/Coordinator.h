#pragma once
#include <atomic>
#include <iostream>
#include <vector>
#include "BlockingQueue.h"
#include "RingBuffer.h"
#include "Acquisition.h"
#include "Uploader.h"

enum class EventKind { PollTick, UploadTick, Shutdown };
struct Event { EventKind kind; };

class Coordinator {
public:
    Coordinator(BlockingQueue<Event>& q,
                RingBuffer<Sample>& buffer,
                AcquisitionScheduler& acq,
                Uploader& upl,
                std::atomic<bool>& running)
        : q_(q), buffer_(buffer), acq_(acq), upl_(upl), running_(running) {}

    void run() {
        idle_ = true; polling_ = false; uploading_ = false;
        poll_ready_ = false; upload_ready_ = false;

        for (;;) {
            if (!running_.load()) break;
            Event ev = q_.pop();
            if (ev.kind == EventKind::Shutdown) break;

            if (ev.kind == EventKind::PollTick) {
                poll_ready_ = true;
                std::cout << "[Poll Timer = 2s] tick → Poll Ready\n";
            } else if (ev.kind == EventKind::UploadTick) {
                upload_ready_ = true;
                std::cout << "[Upload Timer = 15s] tick → Upload Ready\n";
            }
            drain_enabled_transitions();
        }
    }

    void request_stop() { running_.store(false); }

private:
    void drain_enabled_transitions() {
        bool progress = true;
        while (progress) {
            progress = false;

            if (upload_ready_ && !polling_ && !uploading_ && buffer_.not_empty()) {
                std::cout << "Not Polling → Uploading\n";
                do_uploading();
                progress = true;
                continue;
            }
            if (poll_ready_ && !uploading_ && !polling_) {
                std::cout << "Not Uploading → Polling\n";
                do_polling();
                progress = true;
                continue;
            }
        }
    }

    void do_polling() {
        idle_ = false; polling_ = true;

        auto [ok, s] = acq_.poll_once();
        if (!ok) {
            std::cout << "Poll skipped (device unavailable)\n";
            polling_ = false; idle_ = true; return;
        }

        std::cout << "Sample Ready {"
                  << "'t': " << s.t << ", 'voltage': " << s.voltage
                  << ", 'current': " << s.current << ", 'power': " << s.power
                  << "}\n";

        bool pushed = buffer_.push(s);
        if (pushed) {
            std::cout << "Buffer Push\n";
            std::cout << "Pushed | Buffer size = " << buffer_.size() << "\n";
        } else {
            std::cout << "Buffer full — sample dropped\n";
        }

        poll_ready_ = false; polling_ = false; idle_ = true;
        std::cout << "Idle started\n";
    }

    void do_uploading() {
        idle_ = false; uploading_ = true;

        auto batch = buffer_.drain_all();
        std::cout << "Uploading (sending " << batch.size() << " samples)\n";

        bool ok = upl_.upload_once(batch);
        if (ok) {
            std::cout << "Received ACK → Idle\n";
            upload_ready_ = false;
        } else {
            // CloudStub already printed failure; keep behavior consistent:
            for (const auto& s : batch) buffer_.push(s);
            upload_ready_ = false;
        }

        uploading_ = false; idle_ = true;
    }

private:
    BlockingQueue<Event>& q_;
    RingBuffer<Sample>& buffer_;
    AcquisitionScheduler& acq_;
    Uploader& upl_;
    std::atomic<bool>& running_;

    bool idle_{true}, polling_{false}, uploading_{false};
    bool poll_ready_{false}, upload_ready_{false};
};