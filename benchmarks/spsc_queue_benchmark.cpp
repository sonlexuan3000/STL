#include "mini/spsc_queue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <thread>
#include <utility>

#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace {

// This deliberately mirrors the pre-Stage-2 queue: head and tail share a
// cache line, and every operation acquire-loads the foreign index.
template <class T, std::size_t Capacity>
class BaselineSpscQueue {
    static_assert(Capacity > 0);

public:
    BaselineSpscQueue() = default;

    ~BaselineSpscQueue() {
        const std::size_t tail = indices_.tail.load(std::memory_order_relaxed);
        for (std::size_t index =
                 indices_.head.load(std::memory_order_relaxed);
             index != tail;
             index = (index + 1) % slot_count) {
            slot(index)->~T();
        }
    }

    BaselineSpscQueue(const BaselineSpscQueue&) = delete;
    BaselineSpscQueue& operator=(const BaselineSpscQueue&) = delete;

    template <class... Args>
    bool try_emplace(Args&&... args) {
        const std::size_t tail =
            indices_.tail.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) % slot_count;

        if (next == indices_.head.load(std::memory_order_acquire)) {
            return false;
        }

        ::new (raw_slot(tail)) T(std::forward<Args>(args)...);
        indices_.tail.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& output) {
        const std::size_t head =
            indices_.head.load(std::memory_order_relaxed);

        if (head == indices_.tail.load(std::memory_order_acquire)) {
            return false;
        }

        output = std::move(*slot(head));
        slot(head)->~T();
        indices_.head.store((head + 1) % slot_count,
                            std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t slot_count = Capacity + 1;

    void* raw_slot(std::size_t index) noexcept {
        return storage_ + index * sizeof(T);
    }

    T* slot(std::size_t index) noexcept {
        return std::launder(static_cast<T*>(raw_slot(index)));
    }

    struct alignas(128) SharedIndices {
        std::atomic<std::size_t> head{0};
        std::atomic<std::size_t> tail{0};
    };

    alignas(T) std::byte storage_[slot_count * sizeof(T)];
    SharedIndices indices_{};
};

struct Measurement {
    double seconds;
    std::uint64_t checksum;
};

void configure_benchmark_thread() {
#if defined(__APPLE__)
    // Keep both workers at a latency-oriented QoS to reduce P-core/E-core
    // placement noise. macOS still does not promise hard CPU affinity.
    static_cast<void>(
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0));
#endif
}

template <class Queue>
Measurement run_once(std::uint64_t item_count) {
    Queue queue;
    std::atomic<unsigned int> ready{0};
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&] {
        configure_benchmark_thread();
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
        }

        for (std::uint64_t value = 0; value < item_count; ++value) {
            while (!queue.try_emplace(value)) {
            }
        }
    });

    std::thread consumer([&] {
        configure_benchmark_thread();
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
        }

        std::uint64_t output = 0;
        std::uint64_t local_checksum = 0;
        for (std::uint64_t expected = 0; expected < item_count; ++expected) {
            while (!queue.try_pop(output)) {
            }
            if (output != expected) {
                std::abort();
            }
            local_checksum += output;
        }
        checksum = local_checksum;
    });

    while (ready.load(std::memory_order_acquire) != 2) {
        std::this_thread::yield();
    }

    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();

    return {
        std::chrono::duration<double>(end - begin).count(),
        checksum,
    };
}

std::uint64_t expected_checksum(std::uint64_t item_count) {
    if (item_count % 2 == 0) {
        return (item_count / 2) * (item_count - 1);
    }
    return item_count * ((item_count - 1) / 2);
}

template <std::size_t N>
struct SampleSummary {
    double minimum;
    double median;
    double maximum;
};

template <std::size_t N>
SampleSummary<N> summarize(std::array<double, N> samples) {
    std::sort(samples.begin(), samples.end());
    return {samples.front(), samples[N / 2], samples.back()};
}

std::uint64_t parse_item_count(int argc, char** argv) {
    constexpr std::uint64_t default_item_count = 5'000'000;
    if (argc == 1) {
        return default_item_count;
    }
    if (argc != 2) {
        std::cerr << "usage: spsc_queue_benchmark [item-count]\n";
        return 0;
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' || parsed == 0) {
        std::cerr << "item-count must be a positive integer\n";
        return 0;
    }
    return static_cast<std::uint64_t>(parsed);
}

} // namespace

int main(int argc, char** argv) {
    constexpr std::size_t capacity = 1'024;
    constexpr std::size_t trial_count = 7;
    using Baseline = BaselineSpscQueue<std::uint64_t, capacity>;
    using Optimized = mini::SpscQueue<std::uint64_t, capacity>;

    const std::uint64_t item_count = parse_item_count(argc, argv);
    if (item_count == 0) {
        return 2;
    }

    const std::uint64_t warmup_count =
        std::min<std::uint64_t>(item_count, 250'000);
    static_cast<void>(run_once<Baseline>(warmup_count));
    static_cast<void>(run_once<Optimized>(warmup_count));

    std::array<double, trial_count> baseline_samples{};
    std::array<double, trial_count> optimized_samples{};
    const std::uint64_t wanted_checksum = expected_checksum(item_count);

    for (std::size_t trial = 0; trial < trial_count; ++trial) {
        Measurement baseline{};
        Measurement optimized{};

        // Alternate order to reduce systematic thermal/scheduler bias.
        if (trial % 2 == 0) {
            baseline = run_once<Baseline>(item_count);
            optimized = run_once<Optimized>(item_count);
        } else {
            optimized = run_once<Optimized>(item_count);
            baseline = run_once<Baseline>(item_count);
        }

        if (baseline.checksum != wanted_checksum ||
            optimized.checksum != wanted_checksum) {
            std::cerr << "checksum mismatch\n";
            return 1;
        }

        baseline_samples[trial] = baseline.seconds;
        optimized_samples[trial] = optimized.seconds;
    }

    const auto baseline = summarize(baseline_samples);
    const auto optimized = summarize(optimized_samples);
    const double baseline_rate =
        static_cast<double>(item_count) / baseline.median / 1'000'000.0;
    const double optimized_rate =
        static_cast<double>(item_count) / optimized.median / 1'000'000.0;

    std::cout << "SPSC throughput: " << item_count << " transfers, capacity "
              << capacity << ", median of " << trial_count << " trials\n\n"
              << std::fixed << std::setprecision(2)
              << "baseline  " << baseline_rate << " million items/s  ("
              << baseline.median << " s median, " << baseline.minimum
              << "-" << baseline.maximum << " s range)\n"
              << "optimized " << optimized_rate << " million items/s  ("
              << optimized.median << " s median, " << optimized.minimum
              << "-" << optimized.maximum << " s range)\n"
              << "speedup    " << optimized_rate / baseline_rate << "x\n\n"
              << "No CPU affinity is applied; compare medians and rerun before "
                 "drawing conclusions.\n";
}
