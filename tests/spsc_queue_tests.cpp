#include "mini/spsc_queue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#ifndef MINI_SPSC_QUEUE_STAGE
#define MINI_SPSC_QUEUE_STAGE 2
#endif

static_assert(MINI_SPSC_QUEUE_STAGE >= 1 && MINI_SPSC_QUEUE_STAGE <= 2,
              "MINI_SPSC_QUEUE_STAGE must be between 1 and 2");

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << __FILE__ << ':' << __LINE__                          \
                      << ": CHECK failed: " #condition "\n";                 \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

namespace {

struct NoDefault {
    NoDefault() = delete;
    explicit NoDefault(int value_) : value(value_) {}

    NoDefault(const NoDefault&) = delete;
    NoDefault& operator=(const NoDefault&) = delete;
    NoDefault(NoDefault&&) noexcept = default;
    NoDefault& operator=(NoDefault&&) noexcept = default;

    int value;
};

struct Lifetime {
    static inline int alive = 0;
    static inline int constructed = 0;
    static inline int destroyed = 0;

    explicit Lifetime(int value_) : value(value_) {
        ++alive;
        ++constructed;
    }

    Lifetime(const Lifetime&) = delete;
    Lifetime& operator=(const Lifetime&) = delete;

    Lifetime(Lifetime&& other) noexcept : value(other.value) {
        other.value = -1;
        ++alive;
        ++constructed;
    }

    Lifetime& operator=(Lifetime&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~Lifetime() {
        --alive;
        ++destroyed;
    }

    static void reset() {
        CHECK(alive == 0);
        constructed = 0;
        destroyed = 0;
    }

    int value;
};

struct alignas(128) OverAligned {
    explicit OverAligned(int value_ = 0) : value(value_) {
        const auto address = reinterpret_cast<std::uintptr_t>(this);
        CHECK(address % alignof(OverAligned) == 0);
    }

    OverAligned(const OverAligned&) = delete;
    OverAligned& operator=(const OverAligned&) = delete;
    OverAligned(OverAligned&&) noexcept = default;
    OverAligned& operator=(OverAligned&&) noexcept = default;

    int value;
};

struct ThrowOnConstruction {
    static inline bool should_throw = false;

    explicit ThrowOnConstruction(int value_) : value(value_) {
        if (should_throw) {
            throw std::runtime_error("construction failed");
        }
    }

    ThrowOnConstruction(const ThrowOnConstruction&) = delete;
    ThrowOnConstruction& operator=(const ThrowOnConstruction&) = delete;
    ThrowOnConstruction(ThrowOnConstruction&&) noexcept = default;
    ThrowOnConstruction& operator=(ThrowOnConstruction&&) noexcept = default;

    int value;
};

struct Payload {
    explicit Payload(std::uint64_t sequence_ = 0,
                     std::uint64_t checksum_ = 0)
        : sequence(sequence_), checksum(checksum_) {}

    std::uint64_t sequence;
    std::uint64_t checksum;
};

void test_fifo_full_empty_and_wrap() {
    std::cout << "[spsc] FIFO, full/empty, and wrap-around\n";

    using Queue = mini::SpscQueue<int, 3>;
    static_assert(Queue::capacity() == 3);
    static_assert(!std::is_copy_constructible_v<Queue>);
    static_assert(!std::is_move_constructible_v<Queue>);

    Queue queue;
    CHECK(queue.empty());
    CHECK(!queue.full());

    CHECK(queue.try_push(10));
    CHECK(queue.try_emplace(20));
    CHECK(queue.try_push(30));
    CHECK(queue.full());
    CHECK(!queue.try_push(999));

    int output = -1;
    CHECK(queue.try_pop(output));
    CHECK(output == 10);
    CHECK(!queue.full());

    CHECK(queue.try_push(40));
    CHECK(queue.full());

    CHECK(queue.try_pop(output));
    CHECK(output == 20);
    CHECK(queue.try_pop(output));
    CHECK(output == 30);
    CHECK(queue.try_pop(output));
    CHECK(output == 40);
    CHECK(queue.empty());

    output = 777;
    CHECK(!queue.try_pop(output));
    CHECK(output == 777);

    mini::SpscQueue<int, 1> one_slot;
    CHECK(one_slot.try_push(7));
    CHECK(one_slot.full());
    CHECK(!one_slot.try_push(8));
    CHECK(one_slot.try_pop(output));
    CHECK(output == 7);
    CHECK(one_slot.empty());
}

void test_generic_types_and_alignment() {
    std::cout << "[spsc] non-default, move-only, strings, and alignment\n";

    static_assert(std::is_default_constructible_v<
                  mini::SpscQueue<NoDefault, 2>>);

    {
        mini::SpscQueue<NoDefault, 2> queue;
        CHECK(queue.try_emplace(42));

        NoDefault output{0};
        CHECK(queue.try_pop(output));
        CHECK(output.value == 42);
    }

    {
        mini::SpscQueue<std::string, 2> queue;
        const std::string copied = "copied";
        CHECK(queue.try_push(copied));
        CHECK(queue.try_push(std::string{"moved"}));
        CHECK(copied == "copied");

        std::string output;
        CHECK(queue.try_pop(output));
        CHECK(output == "copied");
        CHECK(queue.try_pop(output));
        CHECK(output == "moved");
    }

    {
        mini::SpscQueue<OverAligned, 2> queue;
        CHECK(queue.try_emplace(91));

        OverAligned output;
        CHECK(queue.try_pop(output));
        CHECK(output.value == 91);
    }
}

void test_lifetime_and_destructor_after_wrap() {
    std::cout << "[spsc] raw lifetime and destructor after wrap-around\n";

    Lifetime::reset();
    {
        Lifetime output{-1};
        {
            mini::SpscQueue<Lifetime, 3> queue;
            CHECK(queue.try_emplace(0));
            CHECK(queue.try_emplace(1));
            CHECK(queue.try_emplace(2));
            CHECK(Lifetime::alive == 4); // three slots plus output

            CHECK(queue.try_pop(output));
            CHECK(output.value == 0);
            CHECK(Lifetime::alive == 3); // two slots plus output

            // This emplacement wraps tail back to physical slot three -> zero.
            CHECK(queue.try_emplace(3));
            CHECK(queue.full());
            CHECK(Lifetime::alive == 4);
        }

        // The queue destructor destroyed values 1, 2, and 3.
        CHECK(Lifetime::alive == 1);
        CHECK(output.value == 0);
    }

    CHECK(Lifetime::alive == 0);
    CHECK(Lifetime::constructed == Lifetime::destroyed);
}

void test_failed_construction_does_not_publish() {
    std::cout << "[spsc] throwing construction leaves queue unchanged\n";

    mini::SpscQueue<ThrowOnConstruction, 2> queue;
    ThrowOnConstruction::should_throw = true;

    bool caught = false;
    try {
        static_cast<void>(queue.try_emplace(10));
    } catch (const std::runtime_error&) {
        caught = true;
    }

    CHECK(caught);
    CHECK(queue.empty());

    ThrowOnConstruction::should_throw = false;
    CHECK(queue.try_emplace(20));

    ThrowOnConstruction output{0};
    CHECK(queue.try_pop(output));
    CHECK(output.value == 20);
    CHECK(queue.empty());
}

template <std::size_t Capacity>
void run_concurrent_round_trip(std::uint64_t item_count) {
    mini::SpscQueue<Payload, Capacity> queue;
    std::atomic<bool> start{false};

    std::thread producer([&] {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (std::uint64_t sequence = 0; sequence < item_count; ++sequence) {
            const std::uint64_t checksum = sequence ^ 0x9e3779b97f4a7c15ULL;
            while (!queue.try_emplace(sequence, checksum)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        Payload output;
        start.store(true, std::memory_order_release);

        for (std::uint64_t expected = 0; expected < item_count; ++expected) {
            while (!queue.try_pop(output)) {
                std::this_thread::yield();
            }

            CHECK(output.sequence == expected);
            CHECK(output.checksum ==
                  (expected ^ 0x9e3779b97f4a7c15ULL));
        }
    });

    producer.join();
    consumer.join();
    CHECK(queue.empty());
}

void test_concurrent_producer_consumer() {
    std::cout << "[spsc] concurrent producer/consumer publication\n";
    run_concurrent_round_trip<64>(250'000);

    std::cout << "[spsc] capacity-one slot reuse\n";
    run_concurrent_round_trip<1>(25'000);
}

} // namespace

int main() {
    test_fifo_full_empty_and_wrap();
    test_generic_types_and_alignment();
    test_lifetime_and_destructor_after_wrap();
    test_failed_construction_does_not_publish();
    test_concurrent_producer_consumer();

    std::cout << "all SpscQueue tests passed\n";
}
