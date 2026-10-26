#include "mini/shared_ptr.hpp"
#include "mini/weak_ptr.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef MINI_SHARED_PTR_STAGE
#define MINI_SHARED_PTR_STAGE 1
#endif

static_assert(MINI_SHARED_PTR_STAGE >= 1 && MINI_SHARED_PTR_STAGE <= 9,
              "MINI_SHARED_PTR_STAGE must be between 1 and 9");

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

struct Base {
    static inline int destroyed = 0;

    ~Base() {
        ++destroyed;
    }

    int base_value = 7;
};

struct Derived : Base {
    static inline int destroyed = 0;

    ~Derived() {
        ++destroyed;
    }

    int derived_value = 11;
};

struct LeftBase {
    static inline int destroyed = 0;

    ~LeftBase() {
        ++destroyed;
    }

    int left_value = 13;
};

struct RightBase {
    static inline int destroyed = 0;

    ~RightBase() {
        ++destroyed;
    }

    int right_value = 17;
};

struct MultiplyDerived : LeftBase, RightBase {
    static inline int destroyed = 0;

    ~MultiplyDerived() {
        ++destroyed;
    }
};

struct ConcurrentTracked {
    static inline std::atomic<int> destroyed{0};

    explicit ConcurrentTracked(int value_) : value(value_) {}

    ~ConcurrentTracked() {
        destroyed.fetch_add(1, std::memory_order_relaxed);
    }

    int value;
};

#if MINI_SHARED_PTR_STAGE >= 4
struct alignas(64) MakeSharedTracked {
    static inline int alive = 0;
    static inline int destroyed = 0;

    static void* operator new(std::size_t) = delete;

    MakeSharedTracked(int prefix, std::unique_ptr<int> suffix)
        : value(prefix + *suffix) {
        ++alive;
    }

    ~MakeSharedTracked() {
        --alive;
        ++destroyed;
    }

    std::byte padding[129]{};
    int value;
};
#endif

#if MINI_SHARED_PTR_STAGE >= 5
struct AliasingOwner {
    static inline int alive = 0;
    static inline int destroyed = 0;

    explicit AliasingOwner(std::string name_) : name(std::move(name_)) {
        ++alive;
    }

    ~AliasingOwner() {
        --alive;
        ++destroyed;
    }

    std::string name;
};

static_assert(std::is_constructible_v<
              mini::SharedPtr<std::string>,
              const mini::SharedPtr<AliasingOwner>&,
              std::string*>);
static_assert(std::is_nothrow_constructible_v<
              mini::SharedPtr<std::string>,
              const mini::SharedPtr<AliasingOwner>&,
              std::string*>);
#endif

#if MINI_SHARED_PTR_STAGE >= 6
static_assert(std::is_nothrow_constructible_v<
              mini::SharedPtr<std::string>,
              mini::SharedPtr<AliasingOwner>&&,
              std::string*>);
#endif

#if MINI_SHARED_PTR_STAGE >= 7
static_assert(std::is_same_v<
              decltype(mini::static_pointer_cast<Derived>(
                  std::declval<const mini::SharedPtr<Base>&>())),
              mini::SharedPtr<Derived>>);
static_assert(noexcept(mini::static_pointer_cast<Derived>(
    std::declval<const mini::SharedPtr<Base>&>())));
#endif

#if MINI_SHARED_PTR_STAGE >= 8
struct PolymorphicBase {
    static inline int destroyed = 0;

    virtual ~PolymorphicBase() {
        ++destroyed;
    }

    int base_value = 31;
};

struct PolymorphicDerived final : PolymorphicBase {
    static inline int destroyed = 0;

    ~PolymorphicDerived() override {
        ++destroyed;
    }

    int derived_value = 37;
};

struct PolymorphicSibling final : PolymorphicBase {};

struct PolymorphicLeft {
    virtual ~PolymorphicLeft() = default;
    int left_value = 41;
};

struct PolymorphicRight {
    virtual ~PolymorphicRight() = default;
    int right_value = 43;
};

struct PolymorphicMulti final : PolymorphicLeft, PolymorphicRight {
    static inline int destroyed = 0;

    ~PolymorphicMulti() override {
        ++destroyed;
    }
};

static_assert(std::is_same_v<
              decltype(mini::dynamic_pointer_cast<PolymorphicDerived>(
                  std::declval<const mini::SharedPtr<PolymorphicBase>&>())),
              mini::SharedPtr<PolymorphicDerived>>);
static_assert(noexcept(mini::dynamic_pointer_cast<PolymorphicDerived>(
    std::declval<const mini::SharedPtr<PolymorphicBase>&>())));
#endif

#if MINI_SHARED_PTR_STAGE >= 9
struct CountingTrackedDeleter {
    int* calls;

    void operator()(Tracked* pointer) const noexcept {
        ++*calls;
        delete pointer;
    }
};

struct FreeIntDeleter {
    int* calls;

    void operator()(int* pointer) const noexcept {
        ++*calls;
        std::destroy_at(pointer);
        std::free(pointer);
    }
};

struct NullObservingDeleter {
    int* calls;
    bool* saw_null;

    void operator()(int* pointer) const noexcept {
        ++*calls;
        *saw_null = pointer == nullptr;
        delete pointer;
    }
};

static_assert(std::is_constructible_v<
              mini::SharedPtr<Tracked>, Tracked*, CountingTrackedDeleter>);
#endif

static_assert(std::is_same_v<mini::SharedPtr<int>::element_type, int>);
static_assert(std::is_constructible_v<mini::SharedPtr<Base>, Derived*>);
static_assert(!std::is_constructible_v<mini::SharedPtr<Derived>, Base*>);
static_assert(!std::is_constructible_v<mini::SharedPtr<int>, double*>);
static_assert(!std::has_virtual_destructor_v<Base>);

#if MINI_SHARED_PTR_STAGE >= 2
static_assert(std::is_constructible_v<
              mini::SharedPtr<Base>, const mini::SharedPtr<Derived>&>);
static_assert(std::is_constructible_v<
              mini::SharedPtr<Base>, mini::SharedPtr<Derived>&&>);
static_assert(!std::is_constructible_v<
              mini::SharedPtr<Derived>, const mini::SharedPtr<Base>&>);
static_assert(std::is_assignable_v<
              mini::SharedPtr<Base>&, const mini::SharedPtr<Derived>&>);
#endif

#if MINI_SHARED_PTR_STAGE >= 3
void test_atomic_reference_counting() {
    std::cout << "[stage 3] atomic reference counting\n";
    ConcurrentTracked::destroyed.store(0, std::memory_order_relaxed);

    mini::SharedPtr<ConcurrentTracked> owner(new ConcurrentTracked{73});

    constexpr int thread_count = 8;
    constexpr int copies_per_thread = 2'000;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        mini::SharedPtr<ConcurrentTracked> local_owner(owner);
        workers.emplace_back(
            [local_owner = std::move(local_owner)]() mutable {
                for (int iteration = 0; iteration < copies_per_thread;
                     ++iteration) {
                    mini::SharedPtr<ConcurrentTracked> first(local_owner);
                    mini::SharedPtr<ConcurrentTracked> second(first);
                    CHECK(first->value == 73);
                    second.reset();
                }
            });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    CHECK(owner.use_count() == 1);
    CHECK(ConcurrentTracked::destroyed.load(std::memory_order_relaxed) == 0);

    owner.reset();
    CHECK(ConcurrentTracked::destroyed.load(std::memory_order_relaxed) == 1);
}
#endif

#if MINI_SHARED_PTR_STAGE >= 4
void test_make_shared_inplace_construction_and_weak_lifetime() {
    std::cout << "[stage 4] make_shared in-place construction and lifetime\n";
    CHECK(MakeSharedTracked::alive == 0);
    MakeSharedTracked::destroyed = 0;

    auto argument = std::make_unique<int>(19);
    auto owner = mini::make_shared<MakeSharedTracked>(23, std::move(argument));

    CHECK(argument == nullptr);
    CHECK(owner);
    CHECK(owner->value == 42);
    CHECK(owner.use_count() == 1);
    CHECK(MakeSharedTracked::alive == 1);
    CHECK(MakeSharedTracked::destroyed == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(owner.get()) %
              alignof(MakeSharedTracked) ==
          0);

    mini::WeakPtr<MakeSharedTracked> observer(owner);
    {
        mini::SharedPtr<MakeSharedTracked> copy(owner);
        CHECK(owner.use_count() == 2);
        CHECK(observer.use_count() == 2);
    }

    CHECK(owner.use_count() == 1);
    owner.reset();

    CHECK(MakeSharedTracked::alive == 0);
    CHECK(MakeSharedTracked::destroyed == 1);
    CHECK(observer.expired());
    CHECK(observer.use_count() == 0);
    CHECK(!observer.lock());

    observer.reset();
    CHECK(MakeSharedTracked::destroyed == 1);
}
#endif

#if MINI_SHARED_PTR_STAGE >= 5
void reset_aliasing_counts() {
    CHECK(AliasingOwner::alive == 0);
    AliasingOwner::destroyed = 0;
}

void test_aliasing_member_keeps_owner_alive() {
    std::cout << "[stage 5] aliasing member keeps owner alive\n";
    reset_aliasing_counts();

    auto owner = mini::make_shared<AliasingOwner>("Ada");
    std::string* const name_address = &owner->name;
    mini::SharedPtr<std::string> alias(owner, name_address);

    CHECK(alias.get() == name_address);
    CHECK(*alias == "Ada");
    CHECK(owner.use_count() == 2);
    CHECK(alias.use_count() == 2);
    CHECK(AliasingOwner::alive == 1);

    owner.reset();
    CHECK(AliasingOwner::alive == 1);
    CHECK(AliasingOwner::destroyed == 0);
    CHECK(alias.use_count() == 1);
    CHECK(*alias == "Ada");

    alias.reset();
    CHECK(!alias);
    CHECK(alias.get() == nullptr);
    CHECK(alias.use_count() == 0);
    CHECK(AliasingOwner::alive == 0);
    CHECK(AliasingOwner::destroyed == 1);
}

void test_null_alias_still_owns() {
    std::cout << "[stage 5] null alias still owns\n";
    reset_aliasing_counts();

    auto owner = mini::make_shared<AliasingOwner>("Grace");
    mini::SharedPtr<std::string> alias(owner, nullptr);

    CHECK(!alias);
    CHECK(alias.get() == nullptr);
    CHECK(owner.use_count() == 2);
    CHECK(alias.use_count() == 2);

    owner.reset();
    CHECK(AliasingOwner::alive == 1);
    CHECK(alias.use_count() == 1);

    alias.reset();
    CHECK(AliasingOwner::alive == 0);
    CHECK(AliasingOwner::destroyed == 1);
}

void test_nonnull_alias_with_empty_owner_has_no_ownership() {
    std::cout << "[stage 5] nonnull alias with empty owner\n";

    int external = 91;
    mini::SharedPtr<AliasingOwner> empty_owner;
    mini::SharedPtr<int> alias(empty_owner, &external);

    CHECK(alias);
    CHECK(alias.get() == &external);
    CHECK(*alias == 91);
    CHECK(alias.use_count() == 0);

    alias.reset();
    CHECK(!alias);
    CHECK(alias.get() == nullptr);
    CHECK(alias.use_count() == 0);
    CHECK(external == 91);
}

void test_chained_alias_keeps_original_owner_alive() {
    std::cout << "[stage 5] chained alias keeps original owner alive\n";
    reset_aliasing_counts();

    auto owner = mini::make_shared<AliasingOwner>("Lin");
    mini::SharedPtr<std::string> name(owner, &owner->name);
    mini::SharedPtr<char> first_character(name, name->data());

    CHECK(*first_character == 'L');
    CHECK(owner.use_count() == 3);
    CHECK(name.use_count() == 3);
    CHECK(first_character.use_count() == 3);

    owner.reset();
    name.reset();
    CHECK(AliasingOwner::alive == 1);
    CHECK(AliasingOwner::destroyed == 0);
    CHECK(first_character.use_count() == 1);
    CHECK(*first_character == 'L');

    first_character.reset();
    CHECK(AliasingOwner::alive == 0);
    CHECK(AliasingOwner::destroyed == 1);
}
#endif

#if MINI_SHARED_PTR_STAGE >= 6
void test_move_alias_transfers_ownership_without_increment() {
    std::cout << "[stage 6] move alias transfers ownership\n";
    reset_aliasing_counts();

    auto owner = mini::make_shared<AliasingOwner>("Ken");
    std::string* const name_address = &owner->name;
    CHECK(owner.use_count() == 1);

    mini::SharedPtr<std::string> alias(std::move(owner), name_address);

    CHECK(!owner);
    CHECK(owner.get() == nullptr);
    CHECK(owner.use_count() == 0);
    CHECK(alias.get() == name_address);
    CHECK(*alias == "Ken");
    CHECK(alias.use_count() == 1);
    CHECK(AliasingOwner::alive == 1);

    alias.reset();
    CHECK(AliasingOwner::alive == 0);
    CHECK(AliasingOwner::destroyed == 1);
}

void test_move_alias_from_existing_alias() {
    std::cout << "[stage 6] move alias from existing alias\n";
    reset_aliasing_counts();

    auto owner = mini::make_shared<AliasingOwner>("Jane");
    mini::SharedPtr<std::string> name(owner, &owner->name);
    owner.reset();
    CHECK(name.use_count() == 1);

    char* const first_address = name->data();
    mini::SharedPtr<char> first_character(std::move(name), first_address);

    CHECK(!name);
    CHECK(name.use_count() == 0);
    CHECK(first_character.get() == first_address);
    CHECK(*first_character == 'J');
    CHECK(first_character.use_count() == 1);
    CHECK(AliasingOwner::alive == 1);

    first_character.reset();
    CHECK(AliasingOwner::alive == 0);
    CHECK(AliasingOwner::destroyed == 1);
}

void test_move_null_alias_still_owns() {
    std::cout << "[stage 6] move null alias still owns\n";
    reset_aliasing_counts();

    auto owner = mini::make_shared<AliasingOwner>("Margaret");
    mini::SharedPtr<std::string> alias(std::move(owner), nullptr);

    CHECK(!owner);
    CHECK(owner.use_count() == 0);
    CHECK(!alias);
    CHECK(alias.use_count() == 1);
    CHECK(AliasingOwner::alive == 1);

    alias.reset();
    CHECK(AliasingOwner::alive == 0);
    CHECK(AliasingOwner::destroyed == 1);
}
#endif

#if MINI_SHARED_PTR_STAGE >= 7
void test_static_pointer_cast_downcast_and_lifetime() {
    std::cout << "[stage 7] static_pointer_cast downcast and lifetime\n";
    Base::destroyed = 0;
    Derived::destroyed = 0;

    {
        auto concrete = mini::make_shared<Derived>();
        Derived* const original = concrete.get();
        mini::SharedPtr<Base> base(concrete);
        concrete.reset();
        CHECK(base.use_count() == 1);

        auto recovered = mini::static_pointer_cast<Derived>(base);
        CHECK(recovered.get() == original);
        CHECK(recovered->derived_value == 11);
        CHECK(base.use_count() == 2);
        CHECK(recovered.use_count() == 2);

        base.reset();
        CHECK(recovered.use_count() == 1);
        CHECK(Derived::destroyed == 0);
        CHECK(Base::destroyed == 0);
    }

    CHECK(Derived::destroyed == 1);
    CHECK(Base::destroyed == 1);
}

void test_static_pointer_cast_multiple_inheritance_adjustment() {
    std::cout << "[stage 7] static_pointer_cast pointer adjustment\n";
    LeftBase::destroyed = 0;
    RightBase::destroyed = 0;
    MultiplyDerived::destroyed = 0;

    {
        auto complete = mini::make_shared<MultiplyDerived>();
        MultiplyDerived* const original = complete.get();
        mini::SharedPtr<RightBase> right(complete);
        complete.reset();

        CHECK(right.get() == static_cast<RightBase*>(original));
        CHECK(right.use_count() == 1);

        auto recovered = mini::static_pointer_cast<MultiplyDerived>(right);
        CHECK(recovered.get() == original);
        CHECK(recovered.use_count() == 2);
        CHECK(right.use_count() == 2);

        right.reset();
        CHECK(recovered.use_count() == 1);
    }

    CHECK(MultiplyDerived::destroyed == 1);
    CHECK(LeftBase::destroyed == 1);
    CHECK(RightBase::destroyed == 1);
}

void test_static_pointer_cast_empty_owner() {
    std::cout << "[stage 7] static_pointer_cast empty owner\n";

    mini::SharedPtr<Base> empty;
    auto casted = mini::static_pointer_cast<Derived>(empty);

    CHECK(!casted);
    CHECK(casted.get() == nullptr);
    CHECK(casted.use_count() == 0);
    CHECK(empty.use_count() == 0);
}

void test_static_pointer_cast_typed_null_preserves_ownership() {
    std::cout << "[stage 7] static_pointer_cast typed-null ownership\n";

    Base* null_pointer = nullptr;
    mini::SharedPtr<Base> owner(null_pointer);
    CHECK(!owner);
    CHECK(owner.use_count() == 1);

    auto casted = mini::static_pointer_cast<Derived>(owner);

    CHECK(!casted);
    CHECK(casted.get() == nullptr);
    CHECK(owner.use_count() == 2);
    CHECK(casted.use_count() == 2);

    owner.reset();
    CHECK(casted.use_count() == 1);
    casted.reset();
    CHECK(casted.use_count() == 0);
}
#endif

#if MINI_SHARED_PTR_STAGE >= 8
void test_dynamic_pointer_cast_success_and_failure() {
    std::cout << "[stage 8] dynamic_pointer_cast success and failure\n";
    PolymorphicBase::destroyed = 0;
    PolymorphicDerived::destroyed = 0;

    {
        auto concrete = mini::make_shared<PolymorphicDerived>();
        PolymorphicDerived* const original = concrete.get();
        mini::SharedPtr<PolymorphicBase> base(std::move(concrete));
        CHECK(base.use_count() == 1);

        auto recovered =
            mini::dynamic_pointer_cast<PolymorphicDerived>(base);
        CHECK(recovered.get() == original);
        CHECK(recovered->derived_value == 37);
        CHECK(base.use_count() == 2);
        CHECK(recovered.use_count() == 2);

        recovered.reset();
        CHECK(base.use_count() == 1);

        auto failed = mini::dynamic_pointer_cast<PolymorphicSibling>(base);
        CHECK(!failed);
        CHECK(failed.get() == nullptr);
        CHECK(failed.use_count() == 0);
        CHECK(base.use_count() == 1);
        CHECK(PolymorphicDerived::destroyed == 0);
        CHECK(PolymorphicBase::destroyed == 0);
    }

    CHECK(PolymorphicDerived::destroyed == 1);
    CHECK(PolymorphicBase::destroyed == 1);
}

void test_dynamic_pointer_cast_cross_cast_adjustment() {
    std::cout << "[stage 8] dynamic_pointer_cast cross-cast adjustment\n";
    PolymorphicMulti::destroyed = 0;

    {
        auto complete = mini::make_shared<PolymorphicMulti>();
        PolymorphicMulti* const original = complete.get();
        mini::SharedPtr<PolymorphicLeft> left(complete);
        complete.reset();
        CHECK(left.use_count() == 1);

        auto right = mini::dynamic_pointer_cast<PolymorphicRight>(left);
        CHECK(right.get() == static_cast<PolymorphicRight*>(original));
        CHECK(right->right_value == 43);
        CHECK(left.use_count() == 2);
        CHECK(right.use_count() == 2);

        left.reset();
        CHECK(right.use_count() == 1);
        CHECK(PolymorphicMulti::destroyed == 0);
    }

    CHECK(PolymorphicMulti::destroyed == 1);
}

void test_dynamic_pointer_cast_empty_and_typed_null() {
    std::cout << "[stage 8] dynamic_pointer_cast empty and typed-null\n";

    mini::SharedPtr<PolymorphicBase> empty;
    auto empty_result =
        mini::dynamic_pointer_cast<PolymorphicDerived>(empty);
    CHECK(!empty_result);
    CHECK(empty_result.use_count() == 0);
    CHECK(empty.use_count() == 0);

    PolymorphicBase* null_pointer = nullptr;
    mini::SharedPtr<PolymorphicBase> typed_null(null_pointer);
    CHECK(!typed_null);
    CHECK(typed_null.use_count() == 1);

    auto null_result =
        mini::dynamic_pointer_cast<PolymorphicDerived>(typed_null);
    CHECK(!null_result);
    CHECK(null_result.get() == nullptr);
    CHECK(null_result.use_count() == 0);
    CHECK(typed_null.use_count() == 1);
}
#endif

#if MINI_SHARED_PTR_STAGE >= 9
void reset_tracked_counts();

void test_stateful_custom_deleter_runs_once_at_last_owner() {
    std::cout << "[stage 9] stateful custom deleter\n";
    reset_tracked_counts();
    int deleter_calls = 0;

    {
        mini::SharedPtr<Tracked> owner(
            new Tracked{101}, CountingTrackedDeleter{&deleter_calls});
        mini::SharedPtr<Tracked> copy(owner);

        CHECK(owner.use_count() == 2);
        owner.reset();
        CHECK(deleter_calls == 0);
        CHECK(Tracked::alive == 1);

        copy.reset();
        CHECK(deleter_calls == 1);
        CHECK(Tracked::alive == 0);
        CHECK(Tracked::destroyed == 1);
    }

    CHECK(deleter_calls == 1);
}

void test_custom_deleter_can_release_malloc_storage() {
    std::cout << "[stage 9] malloc/free custom deleter\n";
    int deleter_calls = 0;
    void* const storage = std::malloc(sizeof(int));
    CHECK(storage != nullptr);
    int* const value = std::construct_at(static_cast<int*>(storage), 211);

    {
        mini::SharedPtr<int> owner(
            value, FreeIntDeleter{&deleter_calls});
        CHECK(owner);
        CHECK(*owner == 211);
        CHECK(owner.use_count() == 1);
    }

    CHECK(deleter_calls == 1);
}

void test_typed_null_still_invokes_custom_deleter() {
    std::cout << "[stage 9] typed-null custom deleter\n";
    int deleter_calls = 0;
    bool saw_null = false;
    int* null_pointer = nullptr;

    {
        mini::SharedPtr<int> owner(
            null_pointer,
            NullObservingDeleter{&deleter_calls, &saw_null});
        CHECK(!owner);
        CHECK(owner.use_count() == 1);

        mini::SharedPtr<int> copy(owner);
        owner.reset();
        CHECK(deleter_calls == 0);
        copy.reset();
    }

    CHECK(deleter_calls == 1);
    CHECK(saw_null);
}
#endif

void reset_tracked_counts() {
    CHECK(Tracked::alive == 0);
    Tracked::destroyed = 0;
}

void test_empty_and_typed_null() {
    std::cout << "[stage 1] empty and typed-null ownership\n";

    mini::SharedPtr<int> empty;
    CHECK(!empty);
    CHECK(empty.get() == nullptr);
    CHECK(empty.use_count() == 0);
    empty.reset();
    empty.reset();

    mini::SharedPtr<int> copied_empty(empty);
    CHECK(!copied_empty);
    CHECK(copied_empty.use_count() == 0);

    int* null_pointer = nullptr;
    mini::SharedPtr<int> null_owner(null_pointer);
    CHECK(!null_owner);
    CHECK(null_owner.get() == nullptr);
    CHECK(null_owner.use_count() == 1);

    mini::SharedPtr<int> null_copy(null_owner);
    CHECK(!null_copy);
    CHECK(null_owner.use_count() == 2);
    CHECK(null_copy.use_count() == 2);

    null_copy.reset();
    CHECK(null_copy.use_count() == 0);
    CHECK(null_owner.use_count() == 1);

    mini::SharedPtr<int> null_moved(std::move(null_owner));
    CHECK(null_owner.use_count() == 0);
    CHECK(!null_moved);
    CHECK(null_moved.use_count() == 1);
}

void test_value_semantics_and_lifetime() {
    std::cout << "[stage 1] copy, move, assignment, and lifetime\n";
    reset_tracked_counts();

    {
        mini::SharedPtr<Tracked> first(new Tracked{42});
        CHECK(first);
        CHECK(first.get() != nullptr);
        CHECK(first->value == 42);
        CHECK((*first).value == 42);
        CHECK(first.use_count() == 1);
        CHECK(Tracked::alive == 1);

        mini::SharedPtr<Tracked> second(first);
        CHECK(first.get() == second.get());
        CHECK(first.use_count() == 2);
        CHECK(second.use_count() == 2);

        mini::SharedPtr<Tracked> assigned(new Tracked{5});
        CHECK(Tracked::alive == 2);
        assigned = first;
        CHECK(Tracked::alive == 1);
        CHECK(Tracked::destroyed == 1);
        CHECK(assigned.get() == first.get());
        CHECK(first.use_count() == 3);

        auto& first_alias = first;
        first = first_alias;
        CHECK(first.use_count() == 3);

        mini::SharedPtr<Tracked> moved(std::move(assigned));
        CHECK(!assigned);
        CHECK(assigned.use_count() == 0);
        CHECK(moved.get() == first.get());
        CHECK(moved.use_count() == 3);

        mini::SharedPtr<Tracked> move_assigned(new Tracked{9});
        CHECK(Tracked::alive == 2);
        move_assigned = std::move(moved);
        CHECK(!moved);
        CHECK(Tracked::alive == 1);
        CHECK(Tracked::destroyed == 2);
        CHECK(move_assigned.use_count() == 3);

        second.reset();
        CHECK(!second);
        CHECK(first.use_count() == 2);

        move_assigned.reset();
        CHECK(!move_assigned);
        CHECK(first.use_count() == 1);
        CHECK(Tracked::alive == 1);
    }

    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 3);
}

void test_swap() {
    std::cout << "[stage 1] swap\n";
    reset_tracked_counts();

    {
        mini::SharedPtr<Tracked> left(new Tracked{1});
        mini::SharedPtr<Tracked> right(new Tracked{2});

        Tracked* const left_pointer = left.get();
        Tracked* const right_pointer = right.get();

        left.swap(right);
        CHECK(left.get() == right_pointer);
        CHECK(right.get() == left_pointer);
        CHECK(left->value == 2);
        CHECK(right->value == 1);
        CHECK(left.use_count() == 1);
        CHECK(right.use_count() == 1);

        mini::SharedPtr<Tracked> empty;
        left.swap(empty);
        CHECK(!left);
        CHECK(empty.get() == right_pointer);
        CHECK(empty.use_count() == 1);
    }

    CHECK(Tracked::alive == 0);
    CHECK(Tracked::destroyed == 2);
}

void test_type_erased_deletion() {
    std::cout << "[stage 1] Base view deletes original Derived type\n";
    Base::destroyed = 0;
    Derived::destroyed = 0;

    {
        mini::SharedPtr<Base> owner(new Derived);
        CHECK(owner);
        CHECK(owner->base_value == 7);
        CHECK(owner.use_count() == 1);

        mini::SharedPtr<Base> copy(owner);
        CHECK(owner.use_count() == 2);
        copy.reset();
        CHECK(owner.use_count() == 1);
        CHECK(Base::destroyed == 0);
        CHECK(Derived::destroyed == 0);
    }

    CHECK(Derived::destroyed == 1);
    CHECK(Base::destroyed == 1);
}

#if MINI_SHARED_PTR_STAGE >= 2
void test_converting_copy_and_assignment() {
    std::cout << "[stage 2] converting copy and assignment\n";
    Base::destroyed = 0;
    Derived::destroyed = 0;

    {
        mini::SharedPtr<Derived> derived(new Derived);
        Derived* const original = derived.get();

        mini::SharedPtr<Base> base(derived);
        CHECK(base.get() == static_cast<Base*>(original));
        CHECK(derived.use_count() == 2);
        CHECK(base.use_count() == 2);

        mini::SharedPtr<Base> assigned;
        assigned = derived;
        CHECK(assigned.get() == static_cast<Base*>(original));
        CHECK(derived.use_count() == 3);

        derived.reset();
        CHECK(base.use_count() == 2);
        CHECK(Derived::destroyed == 0);
        CHECK(Base::destroyed == 0);

        assigned.reset();
        CHECK(base.use_count() == 1);
    }

    CHECK(Derived::destroyed == 1);
    CHECK(Base::destroyed == 1);

    mini::SharedPtr<Derived> empty;
    mini::SharedPtr<Base> empty_copy(empty);
    CHECK(!empty_copy);
    CHECK(empty_copy.use_count() == 0);

    Derived* null_pointer = nullptr;
    mini::SharedPtr<Derived> null_owner(null_pointer);
    mini::SharedPtr<Base> null_copy(null_owner);
    CHECK(!null_copy);
    CHECK(null_owner.use_count() == 2);
    CHECK(null_copy.use_count() == 2);
}

void test_converting_move_and_pointer_adjustment() {
    std::cout << "[stage 2] converting move and pointer adjustment\n";
    LeftBase::destroyed = 0;
    RightBase::destroyed = 0;
    MultiplyDerived::destroyed = 0;

    {
        auto* const original = new MultiplyDerived;
        RightBase* const adjusted = static_cast<RightBase*>(original);
        mini::SharedPtr<MultiplyDerived> source(original);

        mini::SharedPtr<RightBase> right(std::move(source));
        CHECK(!source);
        CHECK(source.use_count() == 0);
        CHECK(right.get() == adjusted);
        CHECK(right->right_value == 17);
        CHECK(right.use_count() == 1);
    }

    CHECK(MultiplyDerived::destroyed == 1);
    CHECK(LeftBase::destroyed == 1);
    CHECK(RightBase::destroyed == 1);

    LeftBase::destroyed = 0;
    RightBase::destroyed = 0;
    MultiplyDerived::destroyed = 0;

    {
        auto* const original = new MultiplyDerived;
        RightBase* const adjusted = static_cast<RightBase*>(original);
        mini::SharedPtr<MultiplyDerived> source(original);
        mini::SharedPtr<RightBase> destination(new RightBase);

        destination = std::move(source);
        CHECK(!source);
        CHECK(destination.get() == adjusted);
        CHECK(destination.use_count() == 1);
        CHECK(RightBase::destroyed == 1);
    }

    CHECK(MultiplyDerived::destroyed == 1);
    CHECK(LeftBase::destroyed == 1);
    CHECK(RightBase::destroyed == 2);
}
#endif

} // namespace

int main() {
    test_empty_and_typed_null();
    test_value_semantics_and_lifetime();
    test_swap();
    test_type_erased_deletion();
#if MINI_SHARED_PTR_STAGE >= 2
    test_converting_copy_and_assignment();
    test_converting_move_and_pointer_adjustment();
#endif
#if MINI_SHARED_PTR_STAGE >= 3
    test_atomic_reference_counting();
#endif
#if MINI_SHARED_PTR_STAGE >= 4
    test_make_shared_inplace_construction_and_weak_lifetime();
#endif
#if MINI_SHARED_PTR_STAGE >= 5
    test_aliasing_member_keeps_owner_alive();
    test_null_alias_still_owns();
    test_nonnull_alias_with_empty_owner_has_no_ownership();
    test_chained_alias_keeps_original_owner_alive();
#endif
#if MINI_SHARED_PTR_STAGE >= 6
    test_move_alias_transfers_ownership_without_increment();
    test_move_alias_from_existing_alias();
    test_move_null_alias_still_owns();
#endif
#if MINI_SHARED_PTR_STAGE >= 7
    test_static_pointer_cast_downcast_and_lifetime();
    test_static_pointer_cast_multiple_inheritance_adjustment();
    test_static_pointer_cast_empty_owner();
    test_static_pointer_cast_typed_null_preserves_ownership();
#endif
#if MINI_SHARED_PTR_STAGE >= 8
    test_dynamic_pointer_cast_success_and_failure();
    test_dynamic_pointer_cast_cross_cast_adjustment();
    test_dynamic_pointer_cast_empty_and_typed_null();
#endif
#if MINI_SHARED_PTR_STAGE >= 9
    test_stateful_custom_deleter_runs_once_at_last_owner();
    test_custom_deleter_can_release_malloc_storage();
    test_typed_null_still_invokes_custom_deleter();
#endif

    std::cout << "All enabled mini::SharedPtr tests passed (stage "
              << MINI_SHARED_PTR_STAGE << ")\n";
}
