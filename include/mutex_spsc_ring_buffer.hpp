#pragma once

#include <cstddef>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

// Mutex-protected SPSC ring buffer. Same algorithm as
// LockFreeSPSCRingBuffer (same indexing, same full/empty rule); used as
// a correctness reference and benchmark baseline.
template <typename T>
class MutexSPSCRingBuffer {
public:
    explicit MutexSPSCRingBuffer(size_t min_capacity)
        : capacity_(next_power_of_two(min_capacity + 1)),
          mask_(capacity_ - 1),
          buffer_(static_cast<T*>(::operator new(sizeof(T) * capacity_))) {
        if (min_capacity == 0) {
            throw std::invalid_argument("capacity must be at least 1");
        }
    }

    ~MutexSPSCRingBuffer() {
        while (head_ != tail_) {
            buffer_[head_].~T();
            head_ = (head_ + 1) & mask_;
        }
        ::operator delete(buffer_);
    }

    MutexSPSCRingBuffer(const MutexSPSCRingBuffer&) = delete;
    MutexSPSCRingBuffer& operator=(const MutexSPSCRingBuffer&) = delete;

    bool try_push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t next_tail = (tail_ + 1) & mask_;
        if (next_tail == head_) return false;
        new (&buffer_[tail_]) T(item);
        tail_ = next_tail;
        return true;
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (head_ == tail_) return false;
        out = std::move(buffer_[head_]);
        buffer_[head_].~T();
        head_ = (head_ + 1) & mask_;
        return true;
    }

    size_t capacity() const { return capacity_ - 1; }

private:
    static size_t next_power_of_two(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    const size_t capacity_;
    const size_t mask_;
    T* buffer_;
    std::mutex mutex_;
    size_t head_ = 0;
    size_t tail_ = 0;
};
