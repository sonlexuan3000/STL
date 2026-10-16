#include "mini/priority_queue.hpp"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef MINI_PRIORITY_QUEUE_STAGE
#define MINI_PRIORITY_QUEUE_STAGE 1
#endif

static_assert(MINI_PRIORITY_QUEUE_STAGE >= 1 &&
              MINI_PRIORITY_QUEUE_STAGE <= 3,
              "MINI_PRIORITY_QUEUE_STAGE must be between 1 and 3");

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << __FILE__ << ':' << __LINE__                          \
                      << ": CHECK failed: " #condition "\n";                  \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

namespace {

struct DirectionCompare {
    bool min_heap = false;

    bool operator()(int lhs, int rhs) const noexcept {
        return min_heap ? lhs > rhs : lhs < rhs;
    }
};

struct CountingLess {
    std::size_t* comparisons = nullptr;

    bool operator()(int lhs, int rhs) const noexcept {
        ++*comparisons;
        return lhs < rhs;
    }
};

struct MoveOnly {
    explicit MoveOnly(int value_) : value(value_) {}

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;

    int value;
};

struct MoveOnlyLess {
    bool operator()(const MoveOnly& lhs, const MoveOnly& rhs) const noexcept {
        return lhs.value < rhs.value;
    }
};

void checkpoint_1() {
    std::cout << "[stage 1] max-heap push, emplace, pop\n";
    {
        mini::PriorityQueue<int> queue;
        CHECK(queue.empty());
        CHECK(queue.size() == 0);

        queue.push(4);
        queue.push(-2);
        queue.emplace(9);
        queue.push(9);
        queue.emplace(3);

        const int expected[] = {9, 9, 4, 3, -2};
        for (int value : expected) {
            CHECK(queue.top() == value);
            queue.pop();
        }
        CHECK(queue.empty());
    }

    std::cout << "[stage 1] min-heap and interleaved operations\n";
    {
        using MinQueue = mini::PriorityQueue<
            int, mini::Vector<int>, std::greater<int>>;

        MinQueue queue;
        for (int value : {7, 2, 11, 2, -4}) {
            queue.push(value);
        }

        CHECK(queue.top() == -4);
        queue.pop();
        CHECK(queue.top() == 2);
        queue.push(-8);
        CHECK(queue.top() == -8);
        queue.pop();
        CHECK(queue.top() == 2);
    }
}

#if MINI_PRIORITY_QUEUE_STAGE >= 2
void checkpoint_2() {
    std::cout << "[stage 2] copied and moved container heapification\n";
    {
        mini::Vector<int> source;
        for (int value : {1, 10, 0, 8, 9}) {
            source.push_back(value);
        }

        mini::PriorityQueue<int> copied(std::less<int>{}, source);
        CHECK(source.size() == 5);
        CHECK(source[0] == 1);

        const int expected[] = {10, 9, 8, 1, 0};
        for (int value : expected) {
            CHECK(copied.top() == value);
            copied.pop();
        }

        mini::PriorityQueue<int> moved(std::less<int>{}, std::move(source));
        CHECK(source.empty());
        CHECK(moved.top() == 10);
    }

    std::cout << "[stage 2] linear-time heap construction\n";
    {
        constexpr std::size_t element_count = 4096;
        mini::Vector<int> values;
        for (std::size_t index = 0; index < element_count; ++index) {
            values.push_back(static_cast<int>(index));
        }

        std::size_t comparisons = 0;
        mini::PriorityQueue<int, mini::Vector<int>, CountingLess> queue(
            CountingLess{&comparisons}, std::move(values));

        CHECK(queue.top() == static_cast<int>(element_count - 1));
        CHECK(comparisons < element_count * 4);
    }

    std::cout << "[stage 2] move-only values\n";
    {
        mini::PriorityQueue<
            MoveOnly, mini::Vector<MoveOnly>, MoveOnlyLess> queue;

        queue.emplace(4);
        queue.push(MoveOnly{9});
        queue.emplace(2);
        CHECK(queue.top().value == 9);
        queue.pop();
        CHECK(queue.top().value == 4);

        mini::Vector<MoveOnly> values;
        values.emplace_back(3);
        values.emplace_back(12);
        values.emplace_back(1);

        mini::PriorityQueue<
            MoveOnly, mini::Vector<MoveOnly>, MoveOnlyLess> heapified(
                MoveOnlyLess{}, std::move(values));
        CHECK(heapified.top().value == 12);
    }

    std::cout << "[stage 2] standard-library container\n";
    {
        std::vector<int> values{3, 11, 2, 7};
        mini::PriorityQueue<int, std::vector<int>> queue(
            std::less<int>{}, std::move(values));

        CHECK(queue.top() == 11);
        queue.push(20);
        CHECK(queue.top() == 20);
        queue.pop();
        CHECK(queue.top() == 11);
    }
}
#endif

#if MINI_PRIORITY_QUEUE_STAGE >= 3
template <class Compare>
void differential_test(Compare compare, std::uint32_t seed) {
    mini::PriorityQueue<int, mini::Vector<int>, Compare> actual(compare);
    std::priority_queue<int, std::vector<int>, Compare> expected(compare);

    const auto next_random = [&seed]() {
        seed = seed * 1664525U + 1013904223U;
        return seed;
    };

    for (std::size_t step = 0; step < 20000; ++step) {
        const std::uint32_t operation = next_random() % 100U;

        if (expected.empty() || operation < 65U) {
            const int value = static_cast<int>(next_random() % 401U) - 200;
            if ((operation & 1U) == 0U) {
                actual.push(value);
            } else {
                actual.emplace(value);
            }
            expected.push(value);
        } else {
            actual.pop();
            expected.pop();
        }

        CHECK(actual.empty() == expected.empty());
        CHECK(actual.size() == expected.size());
        if (!expected.empty()) {
            CHECK(actual.top() == expected.top());
        }
    }

    while (!expected.empty()) {
        CHECK(actual.top() == expected.top());
        actual.pop();
        expected.pop();
    }
    CHECK(actual.empty());
}

void checkpoint_3() {
    std::cout << "[stage 3] Rule-of-Zero copy and move\n";
    {
        mini::PriorityQueue<int> original;
        for (int value : {4, 8, 1}) {
            original.push(value);
        }

        mini::PriorityQueue<int> copied = original;
        copied.pop();
        CHECK(original.top() == 8);
        CHECK(copied.top() == 4);

        mini::PriorityQueue<int> copy_assigned;
        copy_assigned.push(-1);
        copy_assigned = original;
        copy_assigned.pop();
        CHECK(original.top() == 8);
        CHECK(copy_assigned.top() == 4);

        mini::PriorityQueue<int> moved = std::move(copied);
        CHECK(moved.top() == 4);

        mini::PriorityQueue<int> move_assigned;
        move_assigned.push(100);
        move_assigned = std::move(copy_assigned);
        CHECK(move_assigned.top() == 4);
    }

    std::cout << "[stage 3] stateful comparator swap\n";
    {
        using StatefulQueue = mini::PriorityQueue<
            int, mini::Vector<int>, DirectionCompare>;

        StatefulQueue maximum(DirectionCompare{false});
        StatefulQueue minimum(DirectionCompare{true});
        for (int value : {1, 3, 2}) {
            maximum.push(value);
            minimum.push(value);
        }

        maximum.swap(minimum);
        CHECK(maximum.top() == 1);
        CHECK(minimum.top() == 3);

        maximum.push(0);
        minimum.push(4);
        CHECK(maximum.top() == 0);
        CHECK(minimum.top() == 4);
    }

    std::cout << "[stage 3] deterministic differential fuzz\n";
    differential_test(std::less<int>{}, 0x13579BDFU);
    differential_test(std::greater<int>{}, 0x2468ACE0U);
}
#endif

} // namespace

int main() {
    checkpoint_1();
#if MINI_PRIORITY_QUEUE_STAGE >= 2
    checkpoint_2();
#endif
#if MINI_PRIORITY_QUEUE_STAGE >= 3
    checkpoint_3();
#endif

    std::cout << "All enabled mini::PriorityQueue tests passed (stage "
              << MINI_PRIORITY_QUEUE_STAGE << ")\n";
}
