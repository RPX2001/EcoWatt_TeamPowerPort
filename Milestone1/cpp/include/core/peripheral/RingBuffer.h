#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include <iostream>

/**
 * @class RingBuffer
 * @brief Thread-safe fixed-capacity FIFO buffer for generic data types.
 *
 * @tparam T Type of elements stored in the buffer.
 *
 * @details
 * This class implements a simple bounded queue with thread-safe
 * push and drain operations. When the buffer is full, new items
 * are rejected and a message is printed to standard output.
 *
 * The buffer is not a circular overwrite buffer — it drops new data
 * when at capacity instead of overwriting the oldest entry.
 */
template <typename T>
class RingBuffer {
    public:
        /**
         * @fn RingBuffer::RingBuffer
         * @brief Construct a ring buffer with a fixed capacity.
         *
         * @param capacity [in] Maximum number of items the buffer can hold.
         */
        explicit RingBuffer(size_t capacity) : cap_(capacity) {}

        /**
         * @fn RingBuffer::push
         * @brief Attempt to insert an item into the buffer.
         *
         * @param item [in] Item to insert.
         * @return true  If the item was successfully inserted.
         * @return false If the buffer was full and the item was dropped.
         *
         * @details
         * - Thread-safe.
         * - If the buffer is at capacity, the item is not inserted and a message
         *   is printed to standard output.
         */
        bool push(const T& item) 
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (q_.size() >= cap_) 
            {
                std::cout << "Buffer full — sample dropped\n";
                return false;
            }
            q_.push_back(item);
            return true;
        }

        /**
         * @fn RingBuffer::drain_all
         * @brief Remove and return all items from the buffer.
         *
         * @return Vector containing all items previously in the buffer.
         *
         * @details
         * Thread-safe. Clears the buffer after copying its contents.
         */
        std::vector<T> drain_all() 
        {
            std::lock_guard<std::mutex> lk(mu_);
            std::vector<T> out(q_.begin(), q_.end());
            q_.clear();
            return out;
        }
    
        /**
         * @fn RingBuffer::not_empty
         * @brief Check whether the buffer contains any items.
         *
         * @return true  If the buffer is not empty.
         * @return false If the buffer is empty.
         *
         * @details Thread-safe.
         */
        bool not_empty() const 
        { 
            std::lock_guard<std::mutex> lk(mu_); return !q_.empty(); 
        }

        /**
         * @fn RingBuffer::size
         * @brief Get the current number of items in the buffer.
         *
         * @return Number of items currently stored in the buffer.
         *
         * @details Thread-safe.
         */
        size_t size() const 
        { 
            std::lock_guard<std::mutex> lk(mu_); return q_.size(); 
        }
    
    private:
        size_t cap_;
        mutable std::mutex mu_;
        std::deque<T> q_;
};
