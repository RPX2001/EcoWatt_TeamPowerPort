#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

/**
 * @class BlockingQueue
 * @brief Thread-safe blocking queue for producer-consumer scenarios.
 *
 * @tparam T Type of elements stored in the queue.
 *
 * @details
 * Provides synchronized push and pop operations, where `pop()` will block
 * until an item is available in the queue.
 */
template <typename T>
class BlockingQueue 
{
    public:
        /**
         * @brief BlockingQueue::push
         * Push an element into the queue.
         *
         * @param v [in] The element to push.
         * @details
         * Thread-safe: locks internally before modifying the queue and notifies
         * one waiting consumer via condition variable.
         */
        void push(const T& v) 
        {
            { 
                std::lock_guard<std::mutex> lk(mu_); q_.push(v); 
            }
            cv_.notify_one();
        }

        /**
         * @brief BlockingQueue::pop
         * Pop an element from the queue.
         *
         * @return T The element removed from the queue.
         *
         * @details
         * Blocks if the queue is empty until an element becomes available.
         * Thread-safe: locks internally for access and waits on a condition variable.
         */
        T pop() 
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&]{ return !q_.empty(); });
            T v = q_.front(); q_.pop(); return v;
        }

    private:
        std::queue<T> q_;
        std::mutex mu_;
        std::condition_variable cv_;
};
