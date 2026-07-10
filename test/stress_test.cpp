// Pushes a strictly increasing sequence through each queue on a real
// producer/consumer thread pair and verifies it comes out identical:
// received[i] == i for every i catches loss, corruption, and reordering
// in a single check.

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "../include/lockfree_spsc_ring_buffer.hpp"
#include "../include/mutex_spsc_ring_buffer.hpp"

namespace {

constexpr size_t kNumItems = 20'000'000;
constexpr size_t kQueueCapacity = 1024;

template <typename QueueT>
bool run_stress_test(const char* label) {
    QueueT queue(kQueueCapacity);
    std::vector<uint64_t> received;
    received.reserve(kNumItems);
    std::atomic<bool> producer_done{false};

    std::thread producer([&queue]() {
        for (uint64_t i = 0; i < kNumItems; ++i) {
            while (!queue.try_push(i)) std::this_thread::yield();
        }
    });

    std::thread consumer([&queue, &received, &producer_done]() {
        uint64_t value;
        while (true) {
            if (queue.try_pop(value)) {
                received.push_back(value);
            } else if (producer_done.load(std::memory_order_relaxed)) {
                if (!queue.try_pop(value)) break;
                received.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    producer_done.store(true, std::memory_order_relaxed);
    consumer.join();

    bool ok = received.size() == kNumItems;
    if (!ok) {
        std::printf("[%s] FAIL: expected %zu items, received %zu\n", label, kNumItems,
                    received.size());
    }

    size_t mismatches = 0;
    for (size_t i = 0; i < received.size() && i < kNumItems; ++i) {
        if (received[i] != i) {
            if (mismatches < 5) {
                std::printf("[%s] FAIL: position %zu expected %zu, got %zu\n", label, i, i,
                            static_cast<size_t>(received[i]));
            }
            ++mismatches;
            ok = false;
        }
    }

    if (ok) {
        std::printf("[%s] PASS: %zu items, no loss, no corruption, no reordering\n", label,
                    kNumItems);
    }
    return ok;
}

}  // namespace

int main() {
    std::printf("Running SPSC stress test with %zu items (queue capacity %zu)\n\n", kNumItems,
                kQueueCapacity);

    bool mutex_ok = run_stress_test<MutexSPSCRingBuffer<uint64_t>>("mutex baseline");
    bool lockfree_ok = run_stress_test<LockFreeSPSCRingBuffer<uint64_t>>("lock-free");

    std::printf("\n%s\n", (mutex_ok && lockfree_ok) ? "ALL TESTS PASSED" : "TESTS FAILED");
    return (mutex_ok && lockfree_ok) ? 0 : 1;
}
