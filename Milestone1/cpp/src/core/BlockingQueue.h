#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue {
public:
    void push(const T& v) {
        { std::lock_guard<std::mutex> lk(mu_); q_.push(v); }
        cv_.notify_one();
    }
    T pop() {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&]{ return !q_.empty(); });
        T v = q_.front(); q_.pop(); return v;
    }
private:
    std::queue<T> q_;
    std::mutex mu_;
    std::condition_variable cv_;
};