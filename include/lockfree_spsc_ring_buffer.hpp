#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <utility>

// Single-producer/single-consumer lock-free ring buffer.
// Exactly one thread may call try_push(); exactly one thread may call
// try_pop(). Not safe for multiple producers or multiple consumers.
template <typename T>
class LockFreeSPSCRingBuffer {
public:
    explicit LockFreeSPSCRingBuffer(size_t min_capacity)
        : capacity_(next_power_of_two(min_capacity + 1)),
          mask_(capacity_ - 1),
          buffer_(static_cast<T*>(::operator new(sizeof(T) * capacity_))) {
        if (min_capacity == 0) {
            throw std::invalid_argument("capacity must be at least 1");
        }
    }

    ~LockFreeSPSCRingBuffer() {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        while (head != tail) {
            buffer_[head].~T();
            head = (head + 1) & mask_;
        }
        ::operator delete(buffer_);
    }

    LockFreeSPSCRingBuffer(const LockFreeSPSCRingBuffer&) = delete;
    LockFreeSPSCRingBuffer& operator=(const LockFreeSPSCRingBuffer&) = delete;

    bool try_push(const T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1) & mask_;

        // acquire: must observe the consumer's prior read of this slot
        // before we overwrite it.
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        new (&buffer_[tail]) T(item);

        // release: publishes the constructed element to the consumer's
        // matching acquire load of tail_ in try_pop.
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) {
        const size_t head = head_.load(std::memory_order_relaxed);

        // acquire: pairs with try_push's release store of tail_.
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        out = std::move(buffer_[head]);
        buffer_[head].~T();

        // release: pairs with try_push's acquire load of head_.
        head_.store((head + 1) & mask_, std::memory_order_release);
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

    // Padded to separate cache lines: written every push/pop by different
    // threads, so co-location would cause false sharing.
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};
