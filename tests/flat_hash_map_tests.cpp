#include "mini/flat_hash_map.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#ifndef MINI_FLAT_HASH_MAP_STAGE
#define MINI_FLAT_HASH_MAP_STAGE 1
#endif

static_assert(MINI_FLAT_HASH_MAP_STAGE >= 1 &&
              MINI_FLAT_HASH_MAP_STAGE <= 3);

namespace {

struct ConstantHash {
    std::size_t operator()(int) const noexcept {
        return 7;
    }
};

struct IdentityHash {
    std::size_t operator()(std::size_t value) const noexcept {
        return value;
    }
};

struct ModHash {
    int modulus;

    std::size_t operator()(int value) const noexcept {
        return static_cast<std::size_t>(value % modulus);
    }
};

struct ModEqual {
    int modulus;

    bool operator()(int lhs, int rhs) const noexcept {
        return lhs % modulus == rhs % modulus;
    }
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

void checkpoint_1() {
    std::cout << "[stage 1] default state, insertion, and lookup\n";
    {
        mini::FlatHashMap<int, std::string> values;
        assert(values.empty());
        assert(values.size() == 0);
        assert(values.capacity() >= 8);
        assert(values.find(1) == nullptr);
        assert(!values.contains(1));

        assert(values.insert(1, "one"));
        assert(values.insert(2, "two"));
        assert(!values.empty());
        assert(values.size() == 2);
        assert(*values.find(1) == "one");
        assert(*values.find(2) == "two");

        assert(!values.insert(1, "replacement"));
        assert(values.size() == 2);
        assert(*values.find(1) == "one");

        const auto& const_values = values;
        const std::string* found = const_values.find(2);
        assert(found != nullptr);
        assert(*found == "two");
        assert(const_values.find(999) == nullptr);
        assert(const_values.contains(1));

        mini::FlatHashMap<int, int> zero_capacity(0);
        assert(zero_capacity.capacity() >= 1);
        assert(zero_capacity.insert(7, 14));
        assert(*zero_capacity.find(7) == 14);
    }

    std::cout << "[stage 1] collisions and probe wrap-around\n";
    {
        mini::FlatHashMap<int, int, ConstantHash> collisions(8);
        for (int key = 0; key < 5; ++key) {
            assert(collisions.insert(key, key * 10));
        }
        assert(collisions.size() == 5);
        for (int key = 0; key < 5; ++key) {
            const int* value = collisions.find(key);
            assert(value != nullptr);
            assert(*value == key * 10);
        }
        assert(collisions.find(100) == nullptr);

        mini::FlatHashMap<std::size_t, int, IdentityHash> wrap(8);
        const std::size_t bucket_count = wrap.capacity();
        assert(bucket_count >= 8);

        const std::size_t first = bucket_count - 1;
        const std::size_t second = first + bucket_count;
        const std::size_t third = second + bucket_count;
        assert(wrap.insert(first, 1));
        assert(wrap.insert(second, 2));
        assert(wrap.insert(third, 3));
        assert(*wrap.find(first) == 1);
        assert(*wrap.find(second) == 2);
        assert(*wrap.find(third) == 3);
        assert(wrap.find(third + bucket_count) == nullptr);
    }
}

#if MINI_FLAT_HASH_MAP_STAGE >= 2

void checkpoint_2() {
    std::cout << "[stage 2] tombstones preserve probe clusters\n";
    {
        mini::FlatHashMap<int, std::string, ConstantHash> values(8);
        assert(values.insert(1, "one"));
        assert(values.insert(2, "two"));
        assert(values.insert(3, "three"));

        assert(values.erase(1));
        assert(values.size() == 2);
        assert(values.find(1) == nullptr);
        assert(*values.find(2) == "two");
        assert(*values.find(3) == "three");

        // There is a tombstone before key 3. Insertion must not stop there and
        // accidentally create a duplicate key 3.
        assert(!values.insert(3, "replacement"));
        assert(values.size() == 2);
        assert(*values.find(3) == "three");

        assert(values.insert(4, "four"));
        assert(values.size() == 3);
        assert(*values.find(4) == "four");

        assert(values.erase(2));
        assert(*values.find(3) == "three");
        assert(*values.find(4) == "four");
        assert(!values.erase(2));
        assert(!values.erase(999));
    }

    std::cout << "[stage 2] clear retains storage and map is reusable\n";
    {
        mini::FlatHashMap<int, int, ConstantHash> values(16);
        for (int key = 0; key < 7; ++key) {
            assert(values.insert(key, key));
        }
        assert(values.erase(1));
        assert(values.erase(3));

        const std::size_t old_capacity = values.capacity();
        values.clear();
        assert(values.empty());
        assert(values.size() == 0);
        assert(values.capacity() == old_capacity);
        for (int key = 0; key < 7; ++key) {
            assert(values.find(key) == nullptr);
        }

        values.clear();
        assert(values.insert(42, 84));
        assert(*values.find(42) == 84);
    }

    std::cout << "[stage 2] erase and clear destroy mapped values\n";
    {
        Lifetime::reset_counts();
        {
            mini::FlatHashMap<int, Lifetime, ConstantHash> values(8);
            assert(values.insert(1, Lifetime{10}));
            assert(values.insert(2, Lifetime{20}));
            assert(Lifetime::alive == 2);

            assert(values.erase(1));
            assert(Lifetime::alive == 1);
            assert(values.find(2)->value == 20);

            values.clear();
            assert(Lifetime::alive == 0);
            assert(values.empty());

            assert(values.insert(3, Lifetime{30}));
            assert(Lifetime::alive == 1);
        }
        assert(Lifetime::alive == 0);
        assert(Lifetime::constructed == Lifetime::destroyed);
    }
}

#endif

#if MINI_FLAT_HASH_MAP_STAGE >= 3

void checkpoint_3() {
    std::cout << "[stage 3] automatic growth, reserve, and rehash\n";
    {
        mini::FlatHashMap<int, int, ConstantHash> values(4);
        const std::size_t initial_capacity = values.capacity();

        for (int key = 0; key < 200; ++key) {
            assert(values.insert(key, key * 3));
        }
        assert(values.size() == 200);
        assert(values.capacity() > initial_capacity);

        for (int key = 0; key < 200; ++key) {
            const int* value = values.find(key);
            assert(value != nullptr);
            assert(*value == key * 3);
        }

        const double expected_load =
            static_cast<double>(values.size()) /
            static_cast<double>(values.capacity());
        assert(std::abs(values.load_factor() - expected_load) < 1e-12);

        const std::size_t requested_capacity = values.capacity() * 2;
        values.rehash(requested_capacity);
        assert(values.capacity() >= requested_capacity);
        for (int key = 0; key < 200; ++key) {
            assert(*values.find(key) == key * 3);
        }

        for (int key = 0; key < 200; key += 2) {
            assert(values.erase(key));
        }
        values.rehash(8);
        assert(values.capacity() >= values.size());
        for (int key = 0; key < 200; ++key) {
            if (key % 2 == 0) {
                assert(values.find(key) == nullptr);
            } else {
                assert(*values.find(key) == key * 3);
            }
        }

        mini::FlatHashMap<int, int> reserved(2);
        for (int key = 0; key < 100; ++key) {
            assert(reserved.insert(key, key + 1));
        }
        reserved.reserve(1000);
        const std::size_t reserved_capacity = reserved.capacity();
        assert(reserved_capacity >= 1000);
        for (int key = 100; key < 1000; ++key) {
            assert(reserved.insert(key, key + 1));
        }
        assert(reserved.size() == 1000);
        assert(reserved.capacity() == reserved_capacity);
        for (int key = 0; key < 1000; ++key) {
            assert(*reserved.find(key) == key + 1);
        }
    }

    std::cout << "[stage 3] copy, move, and swap\n";
    {
        using Map = mini::FlatHashMap<int, std::string>;

        Map source;
        assert(source.insert(1, "one"));
        assert(source.insert(2, "two"));

        Map copy(source);
        assert(*copy.find(1) == "one");
        *copy.find(1) = "ONE";
        assert(*source.find(1) == "one");
        assert(*copy.find(1) == "ONE");

        Map assigned;
        assert(assigned.insert(99, "old"));
        assigned = source;
        assert(assigned.find(99) == nullptr);
        assert(*assigned.find(2) == "two");
        Map& same_assigned = assigned;
        assigned = same_assigned;
        assert(*assigned.find(1) == "one");

        Map moved(std::move(copy));
        assert(copy.empty());
        assert(*moved.find(1) == "ONE");
        assert(copy.insert(7, "reused"));

        Map destination;
        assert(destination.insert(-1, "old destination"));
        destination = std::move(moved);
        assert(moved.empty());
        assert(destination.find(-1) == nullptr);
        assert(*destination.find(2) == "two");
        assert(moved.insert(8, "reused again"));

        Map empty;
        destination.swap(empty);
        assert(destination.empty());
        assert(*empty.find(1) == "ONE");
        empty.swap(empty);
        assert(*empty.find(2) == "two");
    }

    std::cout << "[stage 3] stateful hash and equality objects\n";
    {
        using Map = mini::FlatHashMap<int, std::string, ModHash, ModEqual>;

        Map modulo_ten(8, ModHash{10}, ModEqual{10});
        assert(modulo_ten.insert(1, "one"));
        assert(modulo_ten.contains(11));
        assert(!modulo_ten.insert(21, "duplicate"));
        assert(*modulo_ten.find(31) == "one");

        Map copy(modulo_ten);
        assert(copy.contains(41));

        Map assigned(8, ModHash{7}, ModEqual{7});
        assigned = modulo_ten;
        assert(assigned.contains(51));

        Map modulo_hundred(8, ModHash{100}, ModEqual{100});
        assert(modulo_hundred.insert(2, "two"));
        copy.swap(modulo_hundred);
        assert(copy.contains(102));
        assert(modulo_hundred.contains(11));
    }

    std::cout << "[stage 3] move-only mapped values survive growth\n";
    {
        mini::FlatHashMap<int, std::unique_ptr<int>> values(4);
        for (int key = 0; key < 64; ++key) {
            assert(values.insert(key, std::make_unique<int>(key * 2)));
        }
        for (int key = 0; key < 64; ++key) {
            const auto* value = values.find(key);
            assert(value != nullptr);
            assert(**value == key * 2);
        }
    }

    std::cout << "[stage 3] deterministic differential test\n";
    {
        mini::FlatHashMap<int, int> actual;
        std::unordered_map<int, int> expected;
        std::uint32_t random_state = 0xC0FFEEU;

        const auto next_random = [&random_state]() {
            random_state = random_state * 1664525U + 1013904223U;
            return random_state;
        };

        const auto verify = [&actual, &expected]() {
            assert(actual.size() == expected.size());
            for (int key = 0; key < 97; ++key) {
                const auto expected_it = expected.find(key);
                const int* actual_value = actual.find(key);
                if (expected_it == expected.end()) {
                    assert(actual_value == nullptr);
                } else {
                    assert(actual_value != nullptr);
                    assert(*actual_value == expected_it->second);
                }
            }
        };

        for (int step = 0; step < 5000; ++step) {
            const std::uint32_t random = next_random();
            const int key = static_cast<int>((random >> 8U) % 97U);
            const int value = static_cast<int>(random & 0x7FFFU);
            const unsigned operation = random % 5U;

            if (operation <= 1U) {
                const bool actual_inserted = actual.insert(key, value);
                const bool expected_inserted =
                    expected.emplace(key, value).second;
                assert(actual_inserted == expected_inserted);
            } else if (operation == 2U) {
                const bool actual_erased = actual.erase(key);
                const bool expected_erased = expected.erase(key) != 0;
                assert(actual_erased == expected_erased);
            } else if (operation == 3U) {
                const int* actual_value = actual.find(key);
                const auto expected_it = expected.find(key);
                assert((actual_value == nullptr) ==
                       (expected_it == expected.end()));
                if (actual_value != nullptr) {
                    assert(*actual_value == expected_it->second);
                }
            } else if (step % 101 == 0) {
                actual.clear();
                expected.clear();
            }

            if (step % 53 == 0) {
                verify();
            }
        }
        verify();
    }
}

#endif

} // namespace

int main() {
    checkpoint_1();

#if MINI_FLAT_HASH_MAP_STAGE >= 2
    checkpoint_2();
#endif

#if MINI_FLAT_HASH_MAP_STAGE >= 3
    checkpoint_3();
#endif

    std::cout << "All enabled mini::FlatHashMap tests passed (stage "
              << MINI_FLAT_HASH_MAP_STAGE << ")\n";
}
