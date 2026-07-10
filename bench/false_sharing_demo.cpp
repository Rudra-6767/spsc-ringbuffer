// Two threads each increment their own counter, unpadded vs. padded to
// separate cache lines. The runtime difference is the false-sharing cost.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {

constexpr size_t kIncrements = 200'000'000;

struct UnpaddedCounters {
    std::atomic<uint64_t> a{0};
    std::atomic<uint64_t> b{0};
};

struct PaddedCounters {
    alignas(64) std::atomic<uint64_t> a{0};
    alignas(64) std::atomic<uint64_t> b{0};
};

template <typename Counters>
double run(const char* label) {
    Counters counters;
    auto increment = [](std::atomic<uint64_t>& c) {
        for (size_t i = 0; i < kIncrements; ++i) c.fetch_add(1, std::memory_order_relaxed);
    };

    const auto start = std::chrono::steady_clock::now();
    std::thread t1([&] { increment(counters.a); });
    std::thread t2([&] { increment(counters.b); });
    t1.join();
    t2.join();
    const auto end = std::chrono::steady_clock::now();

    const double elapsed_sec = std::chrono::duration<double>(end - start).count();
    std::printf("%-24s %.3f sec  (%.1f M increments/sec total)\n", label, elapsed_sec,
                (2.0 * kIncrements / elapsed_sec) / 1'000'000.0);
    return elapsed_sec;
}

}  // namespace

int main() {
    std::printf("False sharing demo: two threads, %zu increments each, layout varies.\n\n",
                kIncrements);

    double unpadded_time = run<UnpaddedCounters>("unpadded (false-shared)");
    double padded_time = run<PaddedCounters>("padded (isolated)");

    std::printf("\nPadding made this workload %.2fx faster.\n", unpadded_time / padded_time);
    return 0;
}
