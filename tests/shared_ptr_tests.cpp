#include "mini/shared_ptr.hpp"

#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

#ifndef MINI_SHARED_PTR_STAGE
#define MINI_SHARED_PTR_STAGE 1
#endif

static_assert(MINI_SHARED_PTR_STAGE == 1,
              "MINI_SHARED_PTR_STAGE must be 1");

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

static_assert(std::is_same_v<mini::SharedPtr<int>::element_type, int>);
static_assert(std::is_constructible_v<mini::SharedPtr<Base>, Derived*>);
static_assert(!std::is_constructible_v<mini::SharedPtr<Derived>, Base*>);
static_assert(!std::is_constructible_v<mini::SharedPtr<int>, double*>);
static_assert(!std::has_virtual_destructor_v<Base>);

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

} // namespace

int main() {
    test_empty_and_typed_null();
    test_value_semantics_and_lifetime();
    test_swap();
    test_type_erased_deletion();

    std::cout << "All enabled mini::SharedPtr tests passed (stage "
              << MINI_SHARED_PTR_STAGE << ")\n";
}
