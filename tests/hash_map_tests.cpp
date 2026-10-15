#include "mini/hash_map.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef MINI_HASH_MAP_STAGE
#define MINI_HASH_MAP_STAGE 1
#endif

static_assert(MINI_HASH_MAP_STAGE >= 1 && MINI_HASH_MAP_STAGE <= 2,
              "MINI_HASH_MAP_STAGE must be 1 or 2");

// Unlike assert(), CHECK stays active in Release builds.
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << __FILE__ << ':' << __LINE__                          \
                      << ": CHECK failed: " #condition "\n";                  \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

namespace {

std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27U)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31U);
}

class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() {
        state_ = splitmix64(state_);
        return state_;
    }

    // Uniform-ish value in [0, bound).
    std::uint64_t below(std::uint64_t bound) { return next() % bound; }

private:
    std::uint64_t state_;
};

constexpr std::uint32_t kLiveMagic = 0x11FE11FEU;
constexpr std::uint32_t kDeadMagic = 0xDEADDEADU;

// Key type with operator== and nothing else. Counts constructions,
// destructions, and equality comparisons, and aborts on a double destroy.
class TrackedKey {
public:
    static inline long long alive = 0;
    static inline long long constructed = 0;
    static inline long long equality_calls = 0;

    explicit TrackedKey(std::uint64_t id) : id_(id) { on_construct(); }
    TrackedKey(const TrackedKey& other) : id_(other.id_) { on_construct(); }
    TrackedKey(TrackedKey&& other) noexcept : id_(other.id_) { on_construct(); }
    TrackedKey& operator=(const TrackedKey&) = delete;
    TrackedKey& operator=(TrackedKey&&) = delete;

    ~TrackedKey() {
        CHECK(magic_ == kLiveMagic);  // destroyed twice or never constructed
        magic_ = kDeadMagic;
        --alive;
    }

    std::uint64_t id() const {
        CHECK(magic_ == kLiveMagic);
        return id_;
    }

    friend bool operator==(const TrackedKey& lhs, const TrackedKey& rhs) {
        ++equality_calls;
        return lhs.id() == rhs.id();
    }

    friend bool operator!=(const TrackedKey&, const TrackedKey&) = delete;
    friend bool operator<(const TrackedKey&, const TrackedKey&) = delete;
    friend bool operator>(const TrackedKey&, const TrackedKey&) = delete;
    friend bool operator<=(const TrackedKey&, const TrackedKey&) = delete;
    friend bool operator>=(const TrackedKey&, const TrackedKey&) = delete;

private:
    void on_construct() {
        magic_ = kLiveMagic;
        ++alive;
        ++constructed;
    }

    std::uint64_t id_;
    std::uint32_t magic_ = 0;
};

struct TrackedKeyHash {
    std::size_t operator()(const TrackedKey& key) const {
        return static_cast<std::size_t>(splitmix64(key.id()));
    }
};

class TrackedValue {
public:
    static inline long long alive = 0;
    static inline long long constructed = 0;

    TrackedValue() : value_(0) { on_construct(); }
    explicit TrackedValue(int value) : value_(value) { on_construct(); }
    TrackedValue(const TrackedValue& other) : value_(other.value()) { on_construct(); }
    TrackedValue(TrackedValue&& other) noexcept : value_(other.value()) { on_construct(); }

    TrackedValue& operator=(const TrackedValue& other) {
        CHECK(magic_ == kLiveMagic);
        value_ = other.value();
        return *this;
    }

    ~TrackedValue() {
        CHECK(magic_ == kLiveMagic);
        magic_ = kDeadMagic;
        --alive;
    }

    int value() const {
        CHECK(magic_ == kLiveMagic);
        return value_;
    }

private:
    void on_construct() {
        magic_ = kLiveMagic;
        ++alive;
        ++constructed;
    }

    int value_;
    std::uint32_t magic_ = 0;
};

void reset_tracking() {
    CHECK(TrackedKey::alive == 0);
    CHECK(TrackedValue::alive == 0);
    TrackedKey::constructed = 0;
    TrackedKey::equality_calls = 0;
    TrackedValue::constructed = 0;
}

long long constructions() {
    return TrackedKey::constructed + TrackedValue::constructed;
}

// Every key lands in the same bucket.
struct CollidingHash {
    std::size_t operator()(int) const { return 0; }
};

template <class Map>
void check_load_factor(const Map& map) {
    CHECK(map.size() <= map.bucket_count());
    if (map.bucket_count() == 0) {
        CHECK(map.load_factor() == 0.0F);
    } else {
        const float expected =
            static_cast<float>(map.size()) / static_cast<float>(map.bucket_count());
        const float diff = map.load_factor() - expected;
        CHECK(diff < 1e-4F && diff > -1e-4F);
    }
}

template <class Map, class Model>
void check_same(const Map& map, const Model& model) {
    CHECK(map.size() == model.size());
    CHECK(map.empty() == model.empty());
    check_load_factor(map);
    for (const auto& [key, value] : model) {
        const auto* found = map.find(key);
        CHECK(found != nullptr);
        CHECK(*found == value);
        CHECK(map.contains(key));
    }
}

using TrackedMap = mini::HashMap<TrackedKey, TrackedValue, TrackedKeyHash>;

void checkpoint_1() {
    std::cout << "[stage 1] empty map\n";
    {
        const mini::HashMap<int, int> map;
        CHECK(map.size() == 0);
        CHECK(map.empty());
        CHECK(map.find(42) == nullptr);
        CHECK(!map.contains(42));
        check_load_factor(map);

        mini::HashMap<int, int> mutable_map;
        CHECK(!mutable_map.erase(42));
        mutable_map.clear();
        CHECK(mutable_map.empty());
        CHECK(mutable_map.find(42) == nullptr);
    }

    std::cout << "[stage 1] insert, operator[], find, erase\n";
    {
        mini::HashMap<std::string, int> stock;
        CHECK(stock.insert("apples", 3));
        CHECK(!stock.insert("apples", 9));
        CHECK(*stock.find("apples") == 3);

        stock["pears"] += 2;
        CHECK(*stock.find("pears") == 2);
        CHECK(stock.size() == 2);

        int& pears = stock["pears"];
        CHECK(pears == 2);
        CHECK(&pears == stock.find("pears"));
        CHECK(stock.size() == 2);

        CHECK(stock["plums"] == 0);  // value-initialized
        CHECK(stock.size() == 3);

        CHECK(stock.erase("apples"));
        CHECK(!stock.erase("apples"));
        CHECK(!stock.contains("apples"));
        CHECK(stock.find("apples") == nullptr);
        CHECK(stock.size() == 2);

        const auto& const_stock = stock;
        CHECK(const_stock.find("pears") != nullptr);
        CHECK(*const_stock.find("pears") == 2);

        stock.clear();
        CHECK(stock.empty());
        CHECK(!stock.contains("pears"));
        CHECK(stock.insert("pears", 7));
        CHECK(*stock.find("pears") == 7);
        check_load_factor(stock);
    }

    std::cout << "[stage 1] load factor stays at most 1 and growth is geometric\n";
    {
        mini::HashMap<int, int> map;
        std::size_t previous_buckets = map.bucket_count();
        int bucket_changes = 0;

        for (int key = 0; key < 100000; ++key) {
            if (key % 2 == 0) {
                CHECK(map.insert(key, key * 3));
            } else {
                map[key] = key * 3;
            }
            CHECK(map.size() <= map.bucket_count());
            if (map.bucket_count() != previous_buckets) {
                CHECK(map.bucket_count() > previous_buckets);
                previous_buckets = map.bucket_count();
                ++bucket_changes;
            }
        }
        CHECK(bucket_changes <= 40);
        check_load_factor(map);

        for (int key = 0; key < 100000; ++key) {
            CHECK(*map.find(key) == key * 3);
        }
    }

    std::cout << "[stage 1] reserve\n";
    {
        mini::HashMap<int, int> map;
        map.reserve(1000);
        const std::size_t reserved = map.bucket_count();
        CHECK(reserved >= 1000);

        for (int key = 0; key < 1000; ++key) {
            CHECK(map.insert(key, -key));
            CHECK(map.bucket_count() == reserved);
        }

        map.reserve(10);
        CHECK(map.bucket_count() == reserved);  // never shrinks

        map.reserve(5000);
        CHECK(map.bucket_count() >= 5000);
        CHECK(map.size() == 1000);
        for (int key = 0; key < 1000; ++key) {
            CHECK(*map.find(key) == -key);
        }
        check_load_factor(map);
    }

    std::cout << "[stage 1] keys and values exist only while the map holds them\n";
    reset_tracking();
    {
        TrackedMap map;
        for (std::uint64_t id = 0; id < 2000; ++id) {
            CHECK(map.insert(TrackedKey(id), TrackedValue(static_cast<int>(id))));
            CHECK(TrackedKey::alive == static_cast<long long>(map.size()));
            CHECK(TrackedValue::alive == static_cast<long long>(map.size()));
        }

        {
            const TrackedKey present(7);
            const TrackedKey missing(1000000);
            const TrackedValue value(-1);
            const long long before = constructions();

            CHECK(!map.insert(present, value));  // present: no construction
            CHECK(map.find(present)->value() == 7);
            CHECK(map.contains(present));
            CHECK(map.find(missing) == nullptr);
            CHECK(!map.contains(missing));
            CHECK(!map.erase(missing));
            CHECK(map[present].value() == 7);  // hit: no construction

            CHECK(constructions() == before);
        }

        for (std::uint64_t id = 0; id < 2000; id += 2) {
            CHECK(map.erase(TrackedKey(id)));
            CHECK(TrackedKey::alive == static_cast<long long>(map.size()));
            CHECK(TrackedValue::alive == static_cast<long long>(map.size()));
        }

        {
            const TrackedKey fresh(50000);
            CHECK(map[fresh].value() == 0);  // miss: value-initialized V()
        }
        CHECK(TrackedValue::alive == static_cast<long long>(map.size()));

        map.clear();
        CHECK(map.empty());
        CHECK(TrackedKey::alive == 0);
        CHECK(TrackedValue::alive == 0);

        for (std::uint64_t id = 0; id < 100; ++id) {
            CHECK(map.insert(TrackedKey(id), TrackedValue(1)));
        }
        CHECK(TrackedKey::alive == 100);
    }
    CHECK(TrackedKey::alive == 0);  // destructor releases everything
    CHECK(TrackedValue::alive == 0);

    std::cout << "[stage 1] every key in one bucket\n";
    {
        mini::HashMap<int, int, CollidingHash> map;
        std::unordered_map<int, int> model;

        for (int key = 0; key < 1500; ++key) {
            CHECK(map.insert(key, key + 1));
            model.emplace(key, key + 1);
        }
        for (int key = 0; key < 1500; key += 3) {
            CHECK(map.erase(key));
            model.erase(key);
        }
        for (int key = 0; key < 1500; key += 5) {
            map[key] = -key;
            model[key] = -key;
        }
        check_same(map, model);
        for (int key = 0; key < 1500; key += 3) {
            if (model.count(key) == 0) {
                CHECK(!map.contains(key));
            }
        }
    }

    std::cout << "[stage 1] randomized against std::unordered_map\n";
    {
        mini::HashMap<int, int> map;
        std::unordered_map<int, int> model;
        Rng rng(0xC0FFEE);

        for (int step = 0; step < 200000; ++step) {
            const int key = static_cast<int>(rng.below(5000));
            const int value = static_cast<int>(rng.below(1000000));

            switch (rng.below(8)) {
                case 0:
                case 1: {
                    const bool inserted = model.emplace(key, value).second;
                    CHECK(map.insert(key, value) == inserted);
                    CHECK(map.size() <= map.bucket_count());
                    break;
                }
                case 2:
                    map[key] += value;
                    model[key] += value;
                    CHECK(map.size() <= map.bucket_count());
                    break;
                case 3:
                case 4: {
                    const auto it = model.find(key);
                    const int* found = map.find(key);
                    CHECK((found != nullptr) == (it != model.end()));
                    if (found != nullptr) {
                        CHECK(*found == it->second);
                    }
                    break;
                }
                case 5:
                    CHECK(map.contains(key) == (model.count(key) != 0));
                    break;
                case 6:
                    CHECK(map.erase(key) == (model.erase(key) != 0));
                    break;
                default:
                    if (rng.below(2000) == 0) {
                        map.clear();
                        model.clear();
                    } else if (rng.below(500) == 0) {
                        map.reserve(static_cast<std::size_t>(rng.below(8000)));
                    }
                    break;
            }

            CHECK(map.size() == model.size());
            if (step % 4999 == 0) {
                check_same(map, model);
            }
        }
        check_same(map, model);
    }

    std::cout << "[stage 1] 800,000 operations within a comparison budget\n";
    reset_tracking();
    {
        constexpr std::uint64_t kKeySpace = 200000;
        constexpr long long kOperations = 800000;
        constexpr long long kComparisonsPerOperation = 4;

        TrackedMap map;
        std::vector<bool> present(kKeySpace, false);
        std::size_t expected_size = 0;
        Rng rng(0xBADC0DE);

        for (long long op = 1; op <= kOperations; ++op) {
            const std::uint64_t id = rng.below(kKeySpace);
            const auto index = static_cast<std::size_t>(id);
            const TrackedKey key(id);

            switch (rng.below(4)) {
                case 0: {
                    const bool inserted = map.insert(key, TrackedValue(1));
                    CHECK(inserted == !present[index]);
                    if (inserted) {
                        present[index] = true;
                        ++expected_size;
                    }
                    break;
                }
                case 1:
                    CHECK((map.find(key) != nullptr) == present[index]);
                    break;
                case 2:
                    CHECK(map.erase(key) == present[index]);
                    if (present[index]) {
                        present[index] = false;
                        --expected_size;
                    }
                    break;
                default:
                    CHECK(map.contains(key) == present[index]);
                    break;
            }

            CHECK(map.size() == expected_size);
            if (op % 10000 == 0 &&
                TrackedKey::equality_calls > kComparisonsPerOperation * op) {
                std::cerr << "too many key comparisons: " << TrackedKey::equality_calls
                          << " after " << op << " operations\n";
                std::abort();
            }
        }
    }
    CHECK(TrackedKey::alive == 0);
    CHECK(TrackedValue::alive == 0);
}

#if MINI_HASH_MAP_STAGE >= 2

void checkpoint_2() {
    std::cout << "[stage 2] references survive rehashing and other erasures\n";
    {
        constexpr int kInitial = 64;
        mini::HashMap<int, int> map;

        std::vector<int*> addresses;
        for (int key = 0; key < kInitial; ++key) {
            addresses.push_back(&map[key]);
            *addresses.back() = key * 10;
        }

        // Many rehashes, an explicit reserve, and erasures of other entries.
        for (int key = kInitial; key < 50000; ++key) {
            CHECK(map.insert(key, key));
        }
        map.reserve(200000);
        for (int key = kInitial; key < 50000; key += 2) {
            CHECK(map.erase(key));
        }
        for (int key = 0; key < kInitial; key += 4) {
            CHECK(map.erase(key));
        }

        for (int key = 0; key < kInitial; ++key) {
            if (key % 4 == 0) {
                CHECK(map.find(key) == nullptr);
                continue;
            }
            const auto index = static_cast<std::size_t>(key);
            CHECK(map.find(key) == addresses[index]);
            CHECK(&map[key] == addresses[index]);
            CHECK(*addresses[index] == key * 10);
            *addresses[index] += 1;  // write through the old pointer
            CHECK(*map.find(key) == key * 10 + 1);
        }
    }

    std::cout << "[stage 2] value objects are never relocated\n";
    reset_tracking();
    {
        TrackedMap map;
        const TrackedValue* first = nullptr;
        {
            const TrackedKey key(0);
            CHECK(map.insert(key, TrackedValue(123)));
            first = map.find(key);
        }

        for (std::uint64_t id = 1; id < 20000; ++id) {
            CHECK(map.insert(TrackedKey(id), TrackedValue(0)));
        }
        map.reserve(100000);

        const TrackedKey key(0);
        CHECK(map.find(key) == first);
        CHECK(first->value() == 123);
        CHECK(TrackedValue::alive == static_cast<long long>(map.size()));
    }
    CHECK(TrackedKey::alive == 0);
    CHECK(TrackedValue::alive == 0);
}

#endif

}  // namespace

int main() {
    checkpoint_1();

#if MINI_HASH_MAP_STAGE >= 2
    checkpoint_2();
#endif

    std::cout << "All enabled mini::HashMap tests passed (stage " << MINI_HASH_MAP_STAGE
              << ")\n";
}
