#include "mini/shared_ptr.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <utility>

namespace allocation_test {

std::atomic<bool> fail_next_allocation{false};

} // namespace allocation_test

// Keep this allocation hook in its own executable so it cannot interfere with
// the threaded SharedPtr/WeakPtr tests. It gives us a deterministic way to
// simulate failure while allocating the control block.
void* operator new(std::size_t size) {
    if (allocation_test::fail_next_allocation.exchange(
            false, std::memory_order_relaxed)) {
        throw std::bad_alloc{};
    }

    if (size == 0) {
        size = 1;
    }

    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << __FILE__ << ':' << __LINE__                          \
                      << ": CHECK failed: " #condition "\n";                  \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

namespace {

struct Tracked {
    static inline int alive = 0;
    static inline int destroyed = 0;

    Tracked() {
        ++alive;
    }

    ~Tracked() {
        --alive;
        ++destroyed;
    }
};

struct CountingTrackedDeleter {
    int* calls;

    void operator()(Tracked* pointer) const noexcept {
        ++*calls;
        delete pointer;
    }
};

struct DeleterCopyFailure {};

struct ThrowsOnSecondCopyDeleter {
    static inline int copies = 0;

    explicit ThrowsOnSecondCopyDeleter(int* calls_) noexcept
        : calls{calls_} {}

    ThrowsOnSecondCopyDeleter(
        const ThrowsOnSecondCopyDeleter& other)
        : calls{other.calls} {
        ++copies;
        if (copies == 2) {
            throw DeleterCopyFailure{};
        }
    }

    ThrowsOnSecondCopyDeleter(
        ThrowsOnSecondCopyDeleter&& other) noexcept
        : calls{other.calls} {}

    void operator()(Tracked* pointer) const noexcept {
        ++*calls;
        delete pointer;
    }

    int* calls;
};

void test_raw_pointer_is_deleted_when_control_block_allocation_throws() {
    Tracked::alive = 0;
    Tracked::destroyed = 0;

    Tracked* raw = new Tracked;
    CHECK(Tracked::alive == 1);

    allocation_test::fail_next_allocation.store(true,
                                                 std::memory_order_relaxed);

    bool caught = false;
    try {
        mini::SharedPtr<Tracked> owner(raw);
    } catch (const std::bad_alloc&) {
        caught = true;
    }

    CHECK(caught);
    CHECK(!allocation_test::fail_next_allocation.load(
        std::memory_order_relaxed));
    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 1);
}

void test_next_construction_still_works() {
    {
        mini::SharedPtr<Tracked> owner(new Tracked);
        CHECK(owner);
        CHECK(owner.use_count() == 1);
        CHECK(Tracked::alive == 1);
    }

    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 2);
}

void test_custom_deleter_cleans_up_when_allocation_throws() {
    Tracked::alive = 0;
    Tracked::destroyed = 0;
    int deleter_calls = 0;

    Tracked* raw = new Tracked;
    allocation_test::fail_next_allocation.store(true,
                                                 std::memory_order_relaxed);

    bool caught = false;
    try {
        mini::SharedPtr<Tracked> owner(
            raw, CountingTrackedDeleter{&deleter_calls});
    } catch (const std::bad_alloc&) {
        caught = true;
    }

    CHECK(caught);
    CHECK(deleter_calls == 1);
    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 1);
}

void test_custom_deleter_cleans_up_when_its_copy_throws() {
    Tracked::alive = 0;
    Tracked::destroyed = 0;
    ThrowsOnSecondCopyDeleter::copies = 0;
    int deleter_calls = 0;

    Tracked* raw = new Tracked;
    ThrowsOnSecondCopyDeleter deleter{&deleter_calls};

    bool caught = false;
    try {
        mini::SharedPtr<Tracked> owner(raw, std::move(deleter));
    } catch (const DeleterCopyFailure&) {
        caught = true;
    }

    CHECK(caught);
    CHECK(ThrowsOnSecondCopyDeleter::copies == 2);
    CHECK(deleter_calls == 1);
    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 1);
}

} // namespace

int main() {
    test_raw_pointer_is_deleted_when_control_block_allocation_throws();
    test_next_construction_still_works();
    test_custom_deleter_cleans_up_when_allocation_throws();
    test_custom_deleter_cleans_up_when_its_copy_throws();

    std::cout << "SharedPtr exception tests passed\n";
}
