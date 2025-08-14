#pragma once
#include <deque>
#include <mutex>
#include <vector>
#include <iostream>

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : cap_(capacity) {}

    bool push(const T& item) {
        std::lock_guard<std::mutex> lk(mu_);
        if (q_.size() >= cap_) {
            std::cout << "Buffer full — sample dropped\n";
            return false;
        }
        q_.push_back(item);
        return true;
    }

    std::vector<T> drain_all() {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<T> out(q_.begin(), q_.end());
        q_.clear();
        return out;
    }

    bool not_empty() const { std::lock_guard<std::mutex> lk(mu_); return !q_.empty(); }
    size_t size() const { std::lock_guard<std::mutex> lk(mu_); return q_.size(); }

private:
    size_t cap_;
    mutable std::mutex mu_;
    std::deque<T> q_;
};