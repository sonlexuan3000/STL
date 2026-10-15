#include "mini/list.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef MINI_LIST_STAGE
#define MINI_LIST_STAGE 1
#endif

static_assert(MINI_LIST_STAGE >= 1 && MINI_LIST_STAGE <= 4);

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

struct Record {
    int id;
    std::string name;
};

void checkpoint_1() {
    std::cout << "[stage 1] default state\n";
    {
        mini::List<int> values;
        assert(values.empty());
        assert(values.size() == 0);
    }

    std::cout << "[stage 1] insertion and removal at both ends\n";
    {
        mini::List<int> values;
        values.push_back(2);
        values.push_front(1);
        int& back = values.emplace_back(3);
        int& front = values.emplace_front(0);

        assert(values.size() == 4);
        assert(!values.empty());
        assert(values.front() == 0);
        assert(values.back() == 3);
        assert(&front == &values.front());
        assert(&back == &values.back());

        const mini::List<int>& const_values = values;
        assert(const_values.front() == 0);
        assert(const_values.back() == 3);

        values.pop_front();
        assert(values.size() == 3);
        assert(values.front() == 1);
        assert(values.back() == 3);

        values.pop_back();
        assert(values.size() == 2);
        assert(values.front() == 1);
        assert(values.back() == 2);

        values.pop_front();
        assert(values.size() == 1);
        assert(values.front() == 2);
        assert(values.back() == 2);

        values.pop_back();
        assert(values.empty());
        assert(values.size() == 0);

        values.emplace_front(9);
        assert(values.front() == 9);
        assert(values.back() == 9);
    }

    std::cout << "[stage 1] generic construction and forwarding\n";
    {
        mini::List<NoDefault> no_default;
        no_default.emplace_back(7);
        no_default.emplace_front(6);
        assert(no_default.front().value == 6);
        assert(no_default.back().value == 7);

        mini::List<MoveOnly> move_only;
        move_only.push_back(MoveOnly{20});
        move_only.emplace_front(10);
        assert(move_only.front().value == 10);
        assert(move_only.back().value == 20);

        mini::List<std::string> strings;
        std::string source = "copied";
        strings.push_front(source);
        std::string& result = strings.emplace_back(std::size_t{5}, 'x');
        assert(source == "copied");
        assert(strings.front() == "copied");
        assert(strings.back() == "xxxxx");
        assert(&result == &strings.back());
    }

    std::cout << "[stage 1] lifetime and clear\n";
    {
        Lifetime::reset_counts();
        {
            mini::List<Lifetime> values;
            values.emplace_back(1);
            values.emplace_back(2);
            values.emplace_front(0);
            assert(values.size() == 3);
            assert(Lifetime::alive == 3);

            values.pop_back();
            assert(values.size() == 2);
            assert(Lifetime::alive == 2);

            values.clear();
            assert(values.empty());
            assert(Lifetime::alive == 0);

            values.emplace_back(99);
            assert(Lifetime::alive == 1);
        }
        assert(Lifetime::alive == 0);
        assert(Lifetime::constructed == Lifetime::destroyed);
    }

    std::cout << "[stage 1] over-aligned values\n";
    {
        mini::List<OverAligned> values;
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

#if MINI_LIST_STAGE >= 2

template <class T>
void assert_values(mini::List<T>& actual,
                   std::initializer_list<T> expected) {
    assert(actual.size() == expected.size());
    assert(std::equal(actual.begin(), actual.end(), expected.begin(),
                      expected.end()));
}

void checkpoint_2() {
    using List = mini::List<int>;
    using Iterator = typename List::iterator;

    static_assert(std::bidirectional_iterator<Iterator>);
    static_assert(std::same_as<std::iter_reference_t<Iterator>, int&>);

    std::cout << "[stage 2] empty and singleton iterators\n";
    {
        List values;
        assert(values.begin() == values.end());

        values.emplace_back(42);
        assert(values.begin() != values.end());
        assert(*values.begin() == 42);

        auto last = values.end();
        --last;
        assert(last == values.begin());
        assert(*last == 42);
    }

    std::cout << "[stage 2] forward and backward traversal\n";
    {
        List values;
        for (int value = 1; value <= 5; ++value) {
            values.push_back(value);
        }

        assert_values(values, {1, 2, 3, 4, 5});

        std::vector<int> reverse;
        for (auto it = values.end(); it != values.begin();) {
            --it;
            reverse.push_back(*it);
        }
        assert((reverse == std::vector<int>{5, 4, 3, 2, 1}));

        auto it = values.begin();
        auto old = it++;
        assert(*old == 1);
        assert(*it == 2);
        old = it--;
        assert(*old == 2);
        assert(*it == 1);
    }

    std::cout << "[stage 2] arrow and algorithms\n";
    {
        mini::List<Record> records;
        records.emplace_back(Record{1, "one"});
        records.emplace_back(Record{2, "two"});

        auto it = records.begin();
        assert(it->id == 1);
        it->name = "ONE";

        List numbers;
        numbers.push_back(3);
        numbers.push_back(7);
        numbers.push_back(11);
        auto found = std::find(numbers.begin(), numbers.end(), 7);
        assert(found != numbers.end());
        *found = 8;
        assert_values(numbers, {3, 8, 11});
    }

    std::cout << "[stage 2] insertions preserve existing iterators\n";
    {
        List values;
        values.push_back(10);
        values.push_back(20);
        values.push_back(30);

        auto middle = values.begin();
        ++middle;
        int* original_address = &*middle;

        values.push_front(0);
        values.push_back(40);

        assert(*middle == 20);
        assert(&*middle == original_address);
        assert_values(values, {0, 10, 20, 30, 40});
    }
}

#endif

#if MINI_LIST_STAGE >= 3

void checkpoint_3() {
    using List = mini::List<int>;

    static_assert(std::is_nothrow_move_constructible_v<List>);
    static_assert(std::is_nothrow_move_assignable_v<List>);
    static_assert(std::is_nothrow_swappable_v<List>);

    std::cout << "[stage 3] insert, emplace, and erase\n";
    {
        List values;
        auto one = values.insert(values.end(), 1);
        assert(*one == 1);

        const int three = 3;
        values.insert(values.end(), three);
        auto position = values.begin();
        ++position;
        auto two = values.emplace(position, 2);
        assert(*two == 2);

        values.insert(values.begin(), 0);
        assert_values(values, {0, 1, 2, 3});

        auto next = values.erase(values.begin());
        assert(*next == 1);

        auto middle = values.begin();
        ++middle;
        next = values.erase(middle);
        assert(*next == 3);
        assert_values(values, {1, 3});

        auto last = values.end();
        --last;
        next = values.erase(last);
        assert(next == values.end());
        assert_values(values, {1});
    }

    std::cout << "[stage 3] erase destroys exactly the erased value\n";
    {
        Lifetime::reset_counts();
        {
            mini::List<Lifetime> values;
            values.emplace_back(10);
            values.emplace_back(20);
            values.emplace_back(30);

            assert(Lifetime::alive == 3);
            assert(Lifetime::destroyed == 0);

            auto middle = values.begin();
            ++middle;
            Lifetime* first_address = &values.front();
            Lifetime* last_address = &values.back();

            auto next = values.erase(middle);
            assert(values.size() == 2);
            assert(Lifetime::alive == 2);
            assert(Lifetime::destroyed == 1);
            assert(next->value == 30);
            assert(&values.front() == first_address);
            assert(&values.back() == last_address);

            next = values.erase(values.begin());
            assert(values.size() == 1);
            assert(Lifetime::alive == 1);
            assert(Lifetime::destroyed == 2);
            assert(next == values.begin());
            assert(&*next == last_address);

            next = values.erase(values.begin());
            assert(next == values.end());
            assert(values.empty());
            assert(Lifetime::alive == 0);
            assert(Lifetime::destroyed == 3);
        }
        assert(Lifetime::alive == 0);
        assert(Lifetime::constructed == Lifetime::destroyed);
    }

    std::cout << "[stage 3] deep copy and assignment\n";
    {
        List source;
        source.push_back(1);
        source.push_back(2);
        source.push_back(3);

        List copy(source);
        assert_values(copy, {1, 2, 3});
        *copy.begin() = 99;
        assert_values(source, {1, 2, 3});
        assert_values(copy, {99, 2, 3});

        List assigned;
        assigned.push_back(-1);
        List& result = (assigned = source);
        assert(&result == &assigned);
        assert_values(assigned, {1, 2, 3});

        List& same_object = assigned;
        assigned = same_object;
        assert_values(assigned, {1, 2, 3});
    }

    std::cout << "[stage 3] move and swap\n";
    {
        List source;
        source.push_back(4);
        source.push_back(5);
        int* old_first = &source.front();

        List moved(std::move(source));
        assert(source.empty());
        assert_values(moved, {4, 5});
        assert(&moved.front() == old_first);

        List destination;
        destination.push_back(-1);
        destination = std::move(moved);
        assert(moved.empty());
        assert_values(destination, {4, 5});
        assert(&destination.front() == old_first);

        List other;
        other.push_back(10);
        destination.swap(other);
        assert_values(destination, {10});
        assert_values(other, {4, 5});
    }

}

#endif

#if MINI_LIST_STAGE >= 4

struct ThrowingValue {
    static inline int alive = 0;
    static inline bool should_throw = false;

    explicit ThrowingValue(int value_) : value(value_) {
        if (should_throw) {
            throw std::runtime_error("requested constructor failure");
        }
        ++alive;
    }

    ThrowingValue(const ThrowingValue& other) : value(other.value) {
        if (should_throw) {
            throw std::runtime_error("requested copy failure");
        }
        ++alive;
    }

    ThrowingValue(ThrowingValue&& other) : value(other.value) {
        if (should_throw) {
            throw std::runtime_error("requested move failure");
        }
        other.value = -1;
        ++alive;
    }

    ~ThrowingValue() {
        --alive;
    }

    int value;
};

struct ThrowOnCopy {
    static inline int alive = 0;
    static inline int copies_before_throw = -1;

    explicit ThrowOnCopy(int value_) : value(value_) {
        ++alive;
    }

    ThrowOnCopy(const ThrowOnCopy& other) : value(other.value) {
        if (copies_before_throw == 0) {
            throw std::runtime_error("requested copy failure");
        }
        if (copies_before_throw > 0) {
            --copies_before_throw;
        }
        ++alive;
    }

    ThrowOnCopy(ThrowOnCopy&& other) noexcept : value(other.value) {
        other.value = -1;
        ++alive;
    }

    ThrowOnCopy& operator=(const ThrowOnCopy& other) {
        if (copies_before_throw == 0) {
            throw std::runtime_error("requested copy-assignment failure");
        }
        if (copies_before_throw > 0) {
            --copies_before_throw;
        }
        value = other.value;
        return *this;
    }
    ThrowOnCopy& operator=(ThrowOnCopy&&) noexcept = default;

    ~ThrowOnCopy() {
        --alive;
    }

    int value;
};

void checkpoint_4() {
    std::cout << "[stage 4 bonus] failed construction leaves list unchanged\n";
    {
        assert(ThrowingValue::alive == 0);
        ThrowingValue::should_throw = false;

        mini::List<ThrowingValue> values;
        values.emplace_back(1);
        values.emplace_back(2);
        assert(ThrowingValue::alive == 2);

        ThrowingValue::should_throw = true;
        bool caught = false;
        try {
            values.emplace_back(3);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        ThrowingValue::should_throw = false;

        assert(caught);
        assert(values.size() == 2);
        assert(values.front().value == 1);
        assert(values.back().value == 2);
        assert(ThrowingValue::alive == 2);
    }
    assert(ThrowingValue::alive == 0);

    std::cout << "[stage 4 bonus] copy assignment strong guarantee\n";
    {
        assert(ThrowOnCopy::alive == 0);
        mini::List<ThrowOnCopy> source;
        source.emplace_back(1);
        source.emplace_back(2);
        source.emplace_back(3);

        mini::List<ThrowOnCopy> destination;
        destination.emplace_back(8);
        destination.emplace_back(9);

        ThrowOnCopy::copies_before_throw = 1;
        bool caught = false;
        try {
            destination = source;
        } catch (const std::runtime_error&) {
            caught = true;
        }
        ThrowOnCopy::copies_before_throw = -1;

        assert(caught);
        assert(destination.size() == 2);
        auto it = destination.begin();
        assert(it->value == 8);
        ++it;
        assert(it->value == 9);
        assert(ThrowOnCopy::alive == 5);
    }
    assert(ThrowOnCopy::alive == 0);
}

#endif

} // namespace

int main() {
    checkpoint_1();

#if MINI_LIST_STAGE >= 2
    checkpoint_2();
#endif

#if MINI_LIST_STAGE >= 3
    checkpoint_3();
#endif

#if MINI_LIST_STAGE >= 4
    checkpoint_4();
#endif

    std::cout << "All enabled mini::List tests passed (stage "
              << MINI_LIST_STAGE << ")\n";
}
