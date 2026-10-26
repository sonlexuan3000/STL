#include "mini/weak_ptr.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef MINI_WEAK_PTR_STAGE
#define MINI_WEAK_PTR_STAGE 1
#endif

static_assert(MINI_WEAK_PTR_STAGE >= 1 && MINI_WEAK_PTR_STAGE <= 4,
              "MINI_WEAK_PTR_STAGE must be between 1 and 4");

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

    explicit Tracked(int value_) : value(value_) {
        ++alive;
    }

    ~Tracked() {
        --alive;
        ++destroyed;
    }

    int value;
};

static_assert(std::is_same_v<mini::WeakPtr<int>::element_type, int>);
static_assert(std::is_nothrow_default_constructible_v<mini::WeakPtr<int>>);
static_assert(std::is_nothrow_constructible_v<
              mini::WeakPtr<int>, const mini::SharedPtr<int>&>);
static_assert(std::is_nothrow_copy_constructible_v<mini::WeakPtr<int>>);
static_assert(std::is_nothrow_move_constructible_v<mini::WeakPtr<int>>);

void reset_tracked_counts() {
    CHECK(Tracked::alive == 0);
    Tracked::destroyed = 0;
}

void test_empty_weak_pointer() {
    std::cout << "[stage 1] empty weak pointer\n";

    mini::WeakPtr<int> empty;
    CHECK(empty.use_count() == 0);
    CHECK(empty.expired());

    mini::WeakPtr<int> copied(empty);
    CHECK(copied.use_count() == 0);
    CHECK(copied.expired());

    mini::WeakPtr<int> moved(std::move(copied));
    CHECK(copied.use_count() == 0);
    CHECK(copied.expired());
    CHECK(moved.use_count() == 0);
    CHECK(moved.expired());

    empty.reset();
    empty.reset();
}

void test_observation_does_not_own_object() {
    std::cout << "[stage 1] observation and lifetime split\n";
    reset_tracked_counts();

    mini::WeakPtr<Tracked> observer;

    {
        mini::SharedPtr<Tracked> owner(new Tracked{42});
        observer = owner;

        CHECK(Tracked::alive == 1);
        CHECK(Tracked::destroyed == 0);
        CHECK(owner.use_count() == 1);
        CHECK(observer.use_count() == 1);
        CHECK(!observer.expired());

        mini::WeakPtr<Tracked> copy(observer);
        CHECK(owner.use_count() == 1);
        CHECK(copy.use_count() == 1);
        CHECK(!copy.expired());

        auto& observer_alias = observer;
        observer = observer_alias;
        CHECK(observer.use_count() == 1);
    }

    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 1);
    CHECK(observer.use_count() == 0);
    CHECK(observer.expired());

    observer.reset();
    CHECK(observer.use_count() == 0);
    CHECK(observer.expired());
}

void test_move_reset_and_swap() {
    std::cout << "[stage 1] move, reset, and swap\n";
    reset_tracked_counts();

    mini::SharedPtr<Tracked> first_owner(new Tracked{1});
    mini::SharedPtr<Tracked> second_owner(new Tracked{2});
    mini::WeakPtr<Tracked> first(first_owner);
    mini::WeakPtr<Tracked> second(second_owner);

    first.swap(second);

    first_owner.reset();
    CHECK(second.expired());
    CHECK(second.use_count() == 0);
    CHECK(!first.expired());
    CHECK(first.use_count() == 1);

    mini::WeakPtr<Tracked> moved(std::move(first));
    CHECK(first.expired());
    CHECK(first.use_count() == 0);
    CHECK(!moved.expired());
    CHECK(moved.use_count() == 1);

    moved.reset();
    CHECK(moved.expired());
    CHECK(second_owner.use_count() == 1);

    second_owner.reset();
    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 2);
}

void test_typed_null_is_live_ownership() {
    std::cout << "[stage 1] typed-null ownership\n";

    int* null_pointer = nullptr;
    mini::SharedPtr<int> owner(null_pointer);
    mini::WeakPtr<int> observer(owner);

    CHECK(owner.use_count() == 1);
    CHECK(observer.use_count() == 1);
    CHECK(!observer.expired());

    owner.reset();
    CHECK(observer.use_count() == 0);
    CHECK(observer.expired());
}

#if MINI_WEAK_PTR_STAGE >= 2
void test_lock() {
    std::cout << "[stage 2] lock live and expired ownership\n";
    reset_tracked_counts();

    mini::WeakPtr<Tracked> observer;

    {
        mini::SharedPtr<Tracked> owner(new Tracked{91});
        observer = owner;

        mini::SharedPtr<Tracked> locked = observer.lock();
        CHECK(locked);
        CHECK(locked.get() == owner.get());
        CHECK(locked->value == 91);
        CHECK(owner.use_count() == 2);
        CHECK(observer.use_count() == 2);

        owner.reset();
        CHECK(Tracked::alive == 1);
        CHECK(!observer.expired());

        locked.reset();
        CHECK(Tracked::alive == 0);
        CHECK(observer.expired());
    }

    mini::SharedPtr<Tracked> failed = observer.lock();
    CHECK(!failed);
    CHECK(failed.use_count() == 0);
    CHECK(Tracked::destroyed == 1);
}

void test_lock_empty_and_typed_null() {
    std::cout << "[stage 2] lock empty and typed-null ownership\n";

    mini::WeakPtr<int> empty;
    mini::SharedPtr<int> empty_lock = empty.lock();
    CHECK(!empty_lock);
    CHECK(empty_lock.use_count() == 0);

    int* null_pointer = nullptr;
    mini::SharedPtr<int> null_owner(null_pointer);
    mini::WeakPtr<int> null_observer(null_owner);
    mini::SharedPtr<int> null_lock = null_observer.lock();

    CHECK(!null_lock);
    CHECK(null_lock.get() == nullptr);
    CHECK(null_lock.use_count() == 2);
    CHECK(null_owner.use_count() == 2);
    CHECK(!null_observer.expired());
}
#endif

#if MINI_WEAK_PTR_STAGE >= 3
struct Base {
    int base_value = 7;
};

struct Derived : Base {
    int derived_value = 11;
};

struct LeftBase {
    int left_value = 13;
};

struct RightBase {
    int right_value = 17;
};

struct MultiplyDerived : LeftBase, RightBase {};

static_assert(std::is_constructible_v<
              mini::WeakPtr<Base>, const mini::SharedPtr<Derived>&>);
static_assert(std::is_constructible_v<
              mini::WeakPtr<Base>, const mini::WeakPtr<Derived>&>);
static_assert(!std::is_constructible_v<
              mini::WeakPtr<Derived>, const mini::WeakPtr<Base>&>);

void test_converting_observation_and_pointer_adjustment() {
    std::cout << "[stage 3] converting observation and pointer adjustment\n";

    auto* const original = new MultiplyDerived;
    RightBase* const adjusted = static_cast<RightBase*>(original);
    mini::SharedPtr<MultiplyDerived> owner(original);
    mini::WeakPtr<RightBase> observer(owner);

    mini::SharedPtr<RightBase> locked = observer.lock();
    CHECK(locked.get() == adjusted);
    CHECK(locked->right_value == 17);
    CHECK(owner.use_count() == 2);

    locked.reset();
    owner.reset();
    CHECK(observer.expired());
}

void test_converting_move_and_assignment() {
    std::cout << "[stage 3] converting move and assignment\n";

    mini::SharedPtr<Derived> owner(new Derived);
    mini::WeakPtr<Derived> move_source(owner);
    mini::WeakPtr<Base> moved(std::move(move_source));

    CHECK(move_source.use_count() == 0);
    CHECK(move_source.expired());
    CHECK(moved.use_count() == 1);
    CHECK(!moved.expired());

    mini::SharedPtr<Base> locked = moved.lock();
    CHECK(locked.get() == static_cast<Base*>(owner.get()));
    CHECK(locked->base_value == 7);
    locked.reset();

    mini::WeakPtr<Derived> assignment_source(owner);
    mini::WeakPtr<Base> assigned;
    assigned = std::move(assignment_source);

    CHECK(assignment_source.use_count() == 0);
    CHECK(assignment_source.expired());
    CHECK(assigned.use_count() == 1);

    owner.reset();
    CHECK(moved.expired());
    CHECK(assigned.expired());
}

struct ConcurrentTracked {
    static inline std::atomic<int> destroyed{0};

    explicit ConcurrentTracked(int value_) : value(value_) {}

    ~ConcurrentTracked() {
        destroyed.fetch_add(1, std::memory_order_relaxed);
    }

    int value;
};

void test_lock_races_with_last_owner() {
    std::cout << "[stage 3] lock racing with last owner\n";
    ConcurrentTracked::destroyed.store(0, std::memory_order_relaxed);

    constexpr int round_count = 64;
    constexpr int thread_count = 4;
    constexpr int lock_attempts = 500;

    for (int round = 0; round < round_count; ++round) {
        mini::SharedPtr<ConcurrentTracked> owner(
            new ConcurrentTracked{round});
        mini::WeakPtr<ConcurrentTracked> observer(owner);
        std::atomic<int> ready{0};
        std::atomic<bool> release_held_owners{false};
        std::vector<std::thread> workers;
        workers.reserve(thread_count);

        for (int thread_index = 0; thread_index < thread_count;
             ++thread_index) {
            mini::WeakPtr<ConcurrentTracked> local(observer);
            workers.emplace_back(
                [local = std::move(local), &ready, &release_held_owners,
                 round]() mutable {
                    mini::SharedPtr<ConcurrentTracked> held = local.lock();
                    CHECK(held);
                    CHECK(held->value == round);
                    ready.fetch_add(1, std::memory_order_release);

                    while (!release_held_owners.load(
                        std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }

                    held.reset();
                    for (int attempt = 0; attempt < lock_attempts;
                         ++attempt) {
                        mini::SharedPtr<ConcurrentTracked> locked =
                            local.lock();
                        if (locked) {
                            CHECK(locked->value == round);
                        }
                    }
                });
        }

        while (ready.load(std::memory_order_acquire) != thread_count) {
            std::this_thread::yield();
        }

        owner.reset();
        release_held_owners.store(true, std::memory_order_release);

        for (auto& worker : workers) {
            worker.join();
        }

        CHECK(observer.expired());
        CHECK(!observer.lock());
        CHECK(ConcurrentTracked::destroyed.load(
                  std::memory_order_relaxed) == round + 1);
    }
}
#endif

#if MINI_WEAK_PTR_STAGE >= 4
void test_converting_shared_rvalue_is_only_observed() {
    std::cout << "[stage 4] converting SharedPtr rvalue is only observed\n";

    auto owner = mini::make_shared<MultiplyDerived>();
    MultiplyDerived* const original = owner.get();
    RightBase* const adjusted = static_cast<RightBase*>(original);

    mini::WeakPtr<RightBase> observer(std::move(owner));

    CHECK(owner);
    CHECK(owner.get() == original);
    CHECK(owner.use_count() == 1);
    CHECK(!observer.expired());
    CHECK(observer.use_count() == 1);

    mini::SharedPtr<RightBase> locked = observer.lock();
    CHECK(locked.get() == adjusted);
    CHECK(locked->right_value == 17);
    CHECK(owner.use_count() == 2);

    locked.reset();
    owner.reset();
    CHECK(observer.expired());
}

void test_actual_temporary_shared_owner_expires() {
    std::cout << "[stage 4] actual temporary SharedPtr expires\n";
    reset_tracked_counts();

    mini::WeakPtr<Tracked> observer(mini::make_shared<Tracked>(52));

    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 1);
    CHECK(observer.expired());
    CHECK(observer.use_count() == 0);
    CHECK(!observer.lock());
}

void test_converting_shared_rvalue_assignment_is_only_observed() {
    std::cout << "[stage 4] converting SharedPtr rvalue assignment\n";

    auto owner = mini::make_shared<Derived>();
    Derived* const original = owner.get();
    mini::WeakPtr<Base> observer;

    observer = std::move(owner);

    CHECK(owner);
    CHECK(owner.get() == original);
    CHECK(owner.use_count() == 1);
    CHECK(!observer.expired());
    CHECK(observer.use_count() == 1);

    owner.reset();
    CHECK(observer.expired());
}
#endif

} // namespace

int main() {
    test_empty_weak_pointer();
    test_observation_does_not_own_object();
    test_move_reset_and_swap();
    test_typed_null_is_live_ownership();
#if MINI_WEAK_PTR_STAGE >= 2
    test_lock();
    test_lock_empty_and_typed_null();
#endif
#if MINI_WEAK_PTR_STAGE >= 3
    test_converting_observation_and_pointer_adjustment();
    test_converting_move_and_assignment();
    test_lock_races_with_last_owner();
#endif
#if MINI_WEAK_PTR_STAGE >= 4
    test_converting_shared_rvalue_is_only_observed();
    test_actual_temporary_shared_owner_expires();
    test_converting_shared_rvalue_assignment_is_only_observed();
#endif

    std::cout << "All enabled mini::WeakPtr tests passed (stage "
              << MINI_WEAK_PTR_STAGE << ")\n";
}
