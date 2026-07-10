// Throughput + push-latency percentiles (p50/p99/p999) for both queues.
// Build with -O2/-O3; latency is measured per try_push call including
// any spin-wait, on the producer thread only.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "../include/lockfree_spsc_ring_buffer.hpp"
#include "../include/mutex_spsc_ring_buffer.hpp"

namespace {

using Clock = std::chrono::steady_clock;
constexpr size_t kNumItems = 20'000'000;
constexpr size_t kQueueCapacity = 1024;

struct BenchResult {
    double throughput_million_ops_per_sec;
    double p50_ns, p99_ns, p999_ns;
};

template <typename QueueT>
BenchResult run_benchmark() {
    QueueT queue(kQueueCapacity);
    std::vector<double> push_latencies_ns;
    push_latencies_ns.reserve(kNumItems);

    std::thread consumer([&queue]() {
        uint64_t value;
        size_t popped = 0;
        while (popped < kNumItems) {
            if (queue.try_pop(value)) ++popped;
            else std::this_thread::yield();
        }
    });

    const auto start = Clock::now();
    for (uint64_t i = 0; i < kNumItems; ++i) {
        const auto op_start = Clock::now();
        while (!queue.try_push(i)) std::this_thread::yield();
        const auto op_end = Clock::now();
        push_latencies_ns.push_back(
            std::chrono::duration<double, std::nano>(op_end - op_start).count());
    }
    consumer.join();
    const auto end = Clock::now();

    const double elapsed_sec = std::chrono::duration<double>(end - start).count();
    std::sort(push_latencies_ns.begin(), push_latencies_ns.end());
    auto percentile = [&](double p) {
        size_t idx = std::min(static_cast<size_t>(p * push_latencies_ns.size()),
                              push_latencies_ns.size() - 1);
        return push_latencies_ns[idx];
    };

    return {
        (kNumItems / elapsed_sec) / 1'000'000.0,
        percentile(0.50), percentile(0.99), percentile(0.999),
    };
}

void print_result(const char* label, const BenchResult& r) {
    std::printf("%-16s throughput: %8.2f M ops/sec   p50: %7.1f ns   p99: %8.1f ns   p999: %8.1f ns\n",
                label, r.throughput_million_ops_per_sec, r.p50_ns, r.p99_ns, r.p999_ns);
}

}  // namespace

int main() {
    std::printf("Benchmarking with %zu items (queue capacity %zu)\n\n", kNumItems, kQueueCapacity);

    run_benchmark<MutexSPSCRingBuffer<uint64_t>>();  // warm-up
    BenchResult mutex_result = run_benchmark<MutexSPSCRingBuffer<uint64_t>>();

    run_benchmark<LockFreeSPSCRingBuffer<uint64_t>>();  // warm-up
    BenchResult lockfree_result = run_benchmark<LockFreeSPSCRingBuffer<uint64_t>>();

    print_result("mutex", mutex_result);
    print_result("lock-free", lockfree_result);

    const double throughput_ratio =
        lockfree_result.throughput_million_ops_per_sec / mutex_result.throughput_million_ops_per_sec;
    const double p99_reduction_ns = mutex_result.p99_ns - lockfree_result.p99_ns;

    std::printf("\nSummary: lock-free is %.2fx the throughput of mutex-based, p99 push latency is %.1f ns lower.\n",
                throughput_ratio, p99_reduction_ns);
    return 0;
}
