#include "mini/ring_deque.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#ifndef MINI_RING_DEQUE_STAGE
#define MINI_RING_DEQUE_STAGE 1
#endif

static_assert(MINI_RING_DEQUE_STAGE >= 1 && MINI_RING_DEQUE_STAGE <= 3);

namespace {

struct NoDefault {
    NoDefault() = delete;
    explicit NoDefault(int value_) : value(value_) {}

    int value;
};

struct MoveOnly {
    explicit MoveOnly(int value_) : value(value_) {}

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;

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

    Lifetime(const Lifetime& other) : value(other.value) {
        ++alive;
        ++constructed;
    }

    Lifetime(Lifetime&& other) noexcept : value(other.value) {
        other.value = -1;
        ++alive;
        ++constructed;
    }

    Lifetime& operator=(const Lifetime&) = default;
    Lifetime& operator=(Lifetime&&) noexcept = default;

    ~Lifetime() {
        --alive;
        ++destroyed;
    }

    static void reset_counts() {
        assert(alive == 0);
        constructed = 0;
        destroyed = 0;
    }

    int value;
};

struct alignas(64) OverAligned {
    explicit OverAligned(int value_) : value(value_) {}
    int value;
};

void assert_same(const mini::RingDeque<int>& actual,
                 const std::deque<int>& expected) {
    assert(actual.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        assert(actual[index] == expected[index]);
    }

    if (expected.empty()) {
        assert(actual.empty());
    } else {
        assert(!actual.empty());
        assert(actual.front() == expected.front());
        assert(actual.back() == expected.back());
    }
}

void checkpoint_1() {
    std::cout << "[stage 1] both ends and single-element transitions\n";
    {
        mini::RingDeque<int> values(8);
        assert(values.empty());
        assert(values.size() == 0);
        assert(values.capacity() >= 8);

        values.push_back(2);
        values.push_front(1);
        int& back = values.emplace_back(3);
        int& front = values.emplace_front(0);

        assert(values.size() == 4);
        assert(values.front() == 0);
        assert(values.back() == 3);
        assert(&front == &values.front());
        assert(&back == &values.back());
        assert(values[0] == 0);
        assert(values[1] == 1);
        assert(values[2] == 2);
        assert(values[3] == 3);

        const auto& const_values = values;
        assert(const_values.front() == 0);
        assert(const_values.back() == 3);
        assert(const_values[2] == 2);

        values.pop_front();
        values.pop_back();
        assert(values.front() == 1);
        assert(values.back() == 2);

        values.pop_back();
        assert(values.front() == 1);
        assert(values.back() == 1);
        values.pop_front();
        assert(values.empty());

        values.push_front(9);
        assert(values.front() == 9);
        assert(values.back() == 9);
        values.pop_back();
        assert(values.empty());
    }

    std::cout << "[stage 1] circular wrap-around in both directions\n";
    {
        mini::RingDeque<std::size_t> values(4);
        const std::size_t bucket_count = values.capacity();
        assert(bucket_count >= 4);

        for (std::size_t value = 0; value < bucket_count; ++value) {
            values.push_back(value);
        }

        const std::size_t removed = bucket_count / 2;
        for (std::size_t count = 0; count < removed; ++count) {
            values.pop_front();
        }
        for (std::size_t count = 0; count < removed; ++count) {
            values.push_back(bucket_count + count);
        }

        assert(values.size() == bucket_count);
        for (std::size_t index = 0; index < bucket_count; ++index) {
            assert(values[index] == removed + index);
        }

        for (std::size_t count = 0; count < removed; ++count) {
            values.pop_back();
        }
        for (std::size_t value = removed; value > 0; --value) {
            values.push_front(value - 1);
        }

        assert(values.size() == bucket_count);
        for (std::size_t index = 0; index < bucket_count; ++index) {
            assert(values[index] == index);
        }
    }

    std::cout << "[stage 1] generic construction, lifetime, and alignment\n";
    {
        mini::RingDeque<NoDefault> no_default(4);
        no_default.emplace_back(2);
        no_default.emplace_front(1);
        assert(no_default.front().value == 1);
        assert(no_default.back().value == 2);

        mini::RingDeque<MoveOnly> move_only(4);
        move_only.push_back(MoveOnly{20});
        move_only.emplace_front(10);
        assert(move_only.front().value == 10);
        assert(move_only.back().value == 20);

        mini::RingDeque<std::string> strings(4);
        std::string source = "copied";
        strings.push_front(source);
        strings.emplace_back(std::size_t{5}, 'x');
        assert(source == "copied");
        assert(strings.front() == "copied");
        assert(strings.back() == "xxxxx");
    }

    {
        Lifetime::reset_counts();
        {
            mini::RingDeque<Lifetime> values(4);
            values.emplace_back(1);
            values.emplace_back(2);
            values.emplace_front(0);
            assert(Lifetime::alive == 3);

            values.pop_front();
            assert(Lifetime::alive == 2);
            values.pop_back();
            assert(Lifetime::alive == 1);

            const std::size_t old_capacity = values.capacity();
            values.clear();
            assert(values.empty());
            assert(values.capacity() == old_capacity);
            assert(Lifetime::alive == 0);

            values.emplace_back(99);
            assert(Lifetime::alive == 1);
        }
        assert(Lifetime::alive == 0);
        assert(Lifetime::constructed == Lifetime::destroyed);
    }

    {
        mini::RingDeque<OverAligned> values(4);
        values.emplace_front(1);
        values.emplace_back(2);

        const auto front_address =
            reinterpret_cast<std::uintptr_t>(&values.front());
        const auto back_address =
            reinterpret_cast<std::uintptr_t>(&values.back());
        assert(front_address % alignof(OverAligned) == 0);
        assert(back_address % alignof(OverAligned) == 0);
    }
}

#if MINI_RING_DEQUE_STAGE >= 2

void checkpoint_2() {
    std::cout << "[stage 2] growth from a full wrapped layout\n";
    {
        mini::RingDeque<int> actual(4);
        std::deque<int> expected;
        const std::size_t old_capacity = actual.capacity();
        assert(old_capacity >= 4);

        for (std::size_t index = 0; index < old_capacity; ++index) {
            const int value = static_cast<int>(index);
            actual.push_back(value);
            expected.push_back(value);
        }

        const std::size_t shift = old_capacity / 2;
        for (std::size_t count = 0; count < shift; ++count) {
            actual.pop_front();
            expected.pop_front();
        }
        for (std::size_t count = 0; count < shift; ++count) {
            const int value = static_cast<int>(old_capacity + count);
            actual.push_back(value);
            expected.push_back(value);
        }
        assert(actual.size() == actual.capacity());
        assert_same(actual, expected);

        actual.push_front(-1);
        expected.push_front(-1);
        assert(actual.capacity() > old_capacity);
        assert_same(actual, expected);

        while (actual.size() < actual.capacity()) {
            const int value = static_cast<int>(actual.size() + 1000);
            actual.push_back(value);
            expected.push_back(value);
        }
        const std::size_t second_capacity = actual.capacity();
        actual.push_back(9999);
        expected.push_back(9999);
        assert(actual.capacity() > second_capacity);
        assert_same(actual, expected);
    }

    std::cout << "[stage 2] reserve preserves logical order\n";
    {
        mini::RingDeque<int> actual(8);
        std::deque<int> expected;
        const std::size_t original_capacity = actual.capacity();

        for (std::size_t index = 0; index < original_capacity; ++index) {
            const int value = static_cast<int>(index);
            actual.push_back(value);
            expected.push_back(value);
        }
        for (int count = 0; count < 3; ++count) {
            actual.pop_front();
            expected.pop_front();
        }
        for (int value = 100; value < 103; ++value) {
            actual.push_back(value);
            expected.push_back(value);
        }
        actual.pop_back();
        expected.pop_back();
        assert_same(actual, expected);

        int* existing_address = &actual[1];
        actual.reserve(original_capacity);
        assert(&actual[1] == existing_address);
        assert(actual.capacity() == original_capacity);

        actual.reserve(original_capacity * 3);
        assert(actual.capacity() >= original_capacity * 3);
        assert_same(actual, expected);

        const std::size_t grown_capacity = actual.capacity();
        actual.reserve(1);
        assert(actual.capacity() == grown_capacity);
        assert_same(actual, expected);
    }

    std::cout << "[stage 2] zero capacity, lifetime, and move-only growth\n";
    {
        mini::RingDeque<int> zero_capacity(0);
        assert(zero_capacity.empty());
        zero_capacity.push_back(1);
        zero_capacity.push_front(0);
        assert(zero_capacity.front() == 0);
        assert(zero_capacity.back() == 1);
    }

    {
        Lifetime::reset_counts();
        {
            mini::RingDeque<Lifetime> values(2);
            for (int value = 0; value < 40; ++value) {
                if (value % 2 == 0) {
                    values.emplace_back(value);
                } else {
                    values.emplace_front(value);
                }
                assert(Lifetime::alive == static_cast<int>(values.size()));
            }
            values.reserve(values.capacity() * 2);
            assert(Lifetime::alive == static_cast<int>(values.size()));
            values.clear();
            assert(Lifetime::alive == 0);
        }
        assert(Lifetime::alive == 0);
        assert(Lifetime::constructed == Lifetime::destroyed);
    }

    {
        mini::RingDeque<std::unique_ptr<int>> values(2);
        for (int value = 0; value < 64; ++value) {
            values.push_back(std::make_unique<int>(value));
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            assert(*values[index] == static_cast<int>(index));
        }
    }
}

#endif

#if MINI_RING_DEQUE_STAGE >= 3

void checkpoint_3() {
    std::cout << "[stage 3] copy, move, and swap wrapped storage\n";
    {
        mini::RingDeque<int> source(8);
        std::deque<int> expected;
        const std::size_t source_capacity = source.capacity();

        for (std::size_t index = 0; index < source_capacity; ++index) {
            const int value = static_cast<int>(index);
            source.push_back(value);
            expected.push_back(value);
        }
        for (int count = 0; count < 3; ++count) {
            source.pop_front();
            expected.pop_front();
        }
        for (int value = 100; value < 103; ++value) {
            source.push_back(value);
            expected.push_back(value);
        }
        assert_same(source, expected);

        mini::RingDeque<int> copy(source);
        assert_same(copy, expected);
        copy[0] = -100;
        assert(source[0] == expected[0]);
        std::deque<int> moved_expected = expected;
        moved_expected[0] = -100;

        mini::RingDeque<int> assigned;
        assigned.push_back(-1);
        assigned = source;
        assert_same(assigned, expected);
        mini::RingDeque<int>& same_assigned = assigned;
        assigned = same_assigned;
        assert_same(assigned, expected);

        int* old_first = &copy[0];
        mini::RingDeque<int> moved(std::move(copy));
        assert(copy.empty());
        assert_same(moved, moved_expected);
        assert(&moved[0] == old_first);
        assert(copy.emplace_back(77) == 77);

        mini::RingDeque<int> destination;
        destination.push_back(-1);
        old_first = &moved[0];
        destination = std::move(moved);
        assert(moved.empty());
        assert_same(destination, moved_expected);
        assert(&destination[0] == old_first);
        moved.push_front(88);
        assert(moved.front() == 88);

        mini::RingDeque<int> empty;
        old_first = &destination[0];
        destination.swap(empty);
        assert(destination.empty());
        assert_same(empty, moved_expected);
        assert(&empty[0] == old_first);
        empty.swap(empty);
        assert_same(empty, moved_expected);
        assert(&empty[0] == old_first);

        mini::RingDeque<int>& same_empty = empty;
        empty = std::move(same_empty);
        assert_same(empty, moved_expected);
        assert(&empty[0] == old_first);
    }

    std::cout << "[stage 3] move assignment destroys old destination values\n";
    {
        Lifetime::reset_counts();
        {
            mini::RingDeque<Lifetime> source(4);
            source.emplace_back(1);
            source.emplace_back(2);

            mini::RingDeque<Lifetime> destination(4);
            destination.emplace_back(99);
            assert(Lifetime::alive == 3);

            destination = std::move(source);
            assert(source.empty());
            assert(destination.size() == 2);
            assert(destination.front().value == 1);
            assert(destination.back().value == 2);
            assert(Lifetime::alive == 2);
        }
        assert(Lifetime::alive == 0);
        assert(Lifetime::constructed == Lifetime::destroyed);
    }

    std::cout << "[stage 3] deterministic differential test\n";
    {
        mini::RingDeque<int> actual;
        std::deque<int> expected;
        std::uint32_t random_state = 0xD3C0E123U;

        const auto next_random = [&random_state]() {
            random_state = random_state * 1664525U + 1013904223U;
            return random_state;
        };

        for (int step = 0; step < 20000; ++step) {
            const std::uint32_t random = next_random();
            const int value = static_cast<int>(random & 0x7FFFU);
            const unsigned operation = random % 7U;

            if (operation == 0U) {
                actual.push_back(value);
                expected.push_back(value);
            } else if (operation == 1U) {
                actual.push_front(value);
                expected.push_front(value);
            } else if (operation == 2U && !expected.empty()) {
                actual.pop_back();
                expected.pop_back();
            } else if (operation == 3U && !expected.empty()) {
                actual.pop_front();
                expected.pop_front();
            } else if (operation == 4U && step % 97 == 0) {
                actual.clear();
                expected.clear();
            } else if (operation == 5U) {
                const std::size_t requested =
                    actual.size() + static_cast<std::size_t>(random % 32U);
                actual.reserve(requested);
            } else if (!expected.empty()) {
                const std::size_t index =
                    static_cast<std::size_t>(random) % expected.size();
                actual[index] ^= 1;
                expected[index] ^= 1;
            }

            assert(actual.size() == expected.size());
            if (expected.empty()) {
                assert(actual.empty());
            } else {
                assert(actual.front() == expected.front());
                assert(actual.back() == expected.back());
            }

            if (step % 43 == 0) {
                assert_same(actual, expected);
            }
        }
        assert_same(actual, expected);
    }
}

#endif

} // namespace

int main() {
    checkpoint_1();

#if MINI_RING_DEQUE_STAGE >= 2
    checkpoint_2();
#endif

#if MINI_RING_DEQUE_STAGE >= 3
    checkpoint_3();
#endif

    std::cout << "All enabled mini::RingDeque tests passed (stage "
              << MINI_RING_DEQUE_STAGE << ")\n";
}
