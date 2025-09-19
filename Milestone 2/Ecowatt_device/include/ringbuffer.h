#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#pragma once
#include <Arduino.h>
#include <vector>


// Forward declaration of template
template <typename T, size_t N>
class RingBuffer {
  public:
    RingBuffer();

    void push(const T& item);
    std::vector<T> drain_all();
    bool not_empty() const;
    size_t size() const;
    bool empty() const;
    void clear();

  private:
    T buffer[N];
    size_t head;
    size_t tail;
    bool full;
};

#endif // RINGBUFFER_H