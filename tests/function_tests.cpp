#include "mini/function.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#ifndef MINI_FUNCTION_STAGE
#define MINI_FUNCTION_STAGE 1
#endif

static_assert(MINI_FUNCTION_STAGE >= 1 && MINI_FUNCTION_STAGE <= 4,
              "MINI_FUNCTION_STAGE must be between 1 and 4");

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << __FILE__ << ':' << __LINE__                          \
                      << ": CHECK failed: " #condition "\n";                  \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

namespace {

int add_one(int value) {
    return value + 1;
}

struct Multiplier {
    int factor;

    int operator()(int value) const {
        return factor * value;
    }
};

#if MINI_FUNCTION_STAGE >= 2
struct Counter {
    int value;

    int operator()() {
        return ++value;
    }
};

struct CopyFailure {};

struct ThrowOnCopy {
    static inline bool should_throw = false;

    explicit ThrowOnCopy(int value_) : value{value_} {}

    ThrowOnCopy(const ThrowOnCopy& other) : value{other.value} {
        if (should_throw) {
            throw CopyFailure{};
        }
    }

    ThrowOnCopy(ThrowOnCopy&&) noexcept = default;

    int operator()() const {
        return value;
    }

    int value;
};

struct CopyOnlyCallable {
    explicit CopyOnlyCallable(int offset_) : offset{offset_} {}

    CopyOnlyCallable(const CopyOnlyCallable&) = default;
    CopyOnlyCallable(CopyOnlyCallable&&) = delete;

    int operator()(int value) const {
        return offset + value;
    }

    int offset;
};

static_assert(std::is_copy_constructible_v<CopyOnlyCallable>);
static_assert(!std::is_move_constructible_v<CopyOnlyCallable>);
#endif

#if MINI_FUNCTION_STAGE >= 3
struct NonCallable {};

struct MoveOnlyCallable {
    MoveOnlyCallable() = default;
    MoveOnlyCallable(const MoveOnlyCallable&) = delete;
    MoveOnlyCallable(MoveOnlyCallable&&) noexcept = default;

    int operator()(int value) const {
        return value;
    }
};

struct WrongResultCallable {
    std::string operator()(int) const {
        return "not an int";
    }
};

struct MemberTarget {
    int add(int amount) const {
        return value + amount;
    }

    int value = 37;
};

struct TargetFailure {};

static_assert(std::is_constructible_v<
              mini::Function<int(int)>, Multiplier>);
static_assert(!std::is_constructible_v<
              mini::Function<int(int)>, NonCallable>);
static_assert(!std::is_constructible_v<
              mini::Function<int(int)>, MoveOnlyCallable>);
static_assert(!std::is_constructible_v<
              mini::Function<int(int)>, WrongResultCallable>);
static_assert(!std::is_assignable_v<
              mini::Function<int(int)>&, NonCallable>);
#endif

void test_empty_state_and_bad_function_call() {
    mini::Function<int(int)> function;

    CHECK(!function);

    bool caught = false;
    try {
        (void)function(3);
    } catch (const std::bad_function_call&) {
        caught = true;
    }
    CHECK(caught);
}

void test_function_pointer_functor_and_lambda() {
    mini::Function<int(int)> pointer_target(&add_one);
    CHECK(pointer_target);
    CHECK(pointer_target(8) == 9);

    mini::Function<int(int)> decayed_function_target(add_one);
    CHECK(decayed_function_target(40) == 41);

    mini::Function<int(int)> functor_target(Multiplier{6});
    CHECK(functor_target(7) == 42);

    int offset = 11;
    mini::Function<int(int)> lambda_target(
        [offset](int value) { return offset + value; });
    CHECK(lambda_target(31) == 42);
}

void test_mutable_lambda_and_void_return() {
    mini::Function<int()> counter(
        [value = 0]() mutable { return ++value; });
    CHECK(counter() == 1);
    CHECK(counter() == 2);

    int observed = 0;
    mini::Function<void(int)> sink(
        [&observed](int value) { observed = value; });
    sink(42);
    CHECK(observed == 42);
}

#if MINI_FUNCTION_STAGE >= 2
void test_copy_constructor_clones_callable_state() {
    mini::Function<int()> original(Counter{0});
    CHECK(original() == 1);

    mini::Function<int()> copy(original);

    CHECK(original() == 2);
    CHECK(copy() == 2);
    CHECK(original() == 3);
    CHECK(copy() == 3);
}

void test_empty_copy_and_move_constructor() {
    mini::Function<int()> empty;
    mini::Function<int()> copied(empty);
    CHECK(!empty);
    CHECK(!copied);

    mini::Function<int()> moved(std::move(empty));
    CHECK(!empty);
    CHECK(!moved);
}

void test_move_constructor_transfers_target() {
    mini::Function<int()> source(Counter{40});
    mini::Function<int()> destination(std::move(source));

    CHECK(!source);
    CHECK(destination);
    CHECK(destination() == 41);
}

void test_copy_and_move_assignment() {
    mini::Function<int()> source(Counter{10});
    CHECK(source() == 11);

    mini::Function<int()> copy_target(Counter{90});
    copy_target = source;
    CHECK(source() == 12);
    CHECK(copy_target() == 12);

    auto* const copy_self = &copy_target;
    copy_target = *copy_self;
    CHECK(copy_target() == 13);

    mini::Function<int()> move_target(Counter{200});
    move_target = std::move(source);
    CHECK(!source);
    CHECK(move_target() == 13);

    auto* const move_self = &move_target;
    move_target = std::move(*move_self);
    CHECK(move_target);
    CHECK(move_target() == 14);
}

void test_null_callable_assignment_and_swap() {
    mini::Function<int(int)> function(Multiplier{2});
    function = Multiplier{7};
    CHECK(function(6) == 42);

    function = nullptr;
    CHECK(!function);

    mini::Function<int(int)> left(Multiplier{3});
    mini::Function<int(int)> right(Multiplier{5});
    left.swap(right);
    CHECK(left(4) == 20);
    CHECK(right(4) == 12);

    mini::swap(left, right);
    CHECK(left(4) == 12);
    CHECK(right(4) == 20);
}

void test_copy_assignment_has_strong_exception_guarantee() {
    ThrowOnCopy::should_throw = false;
    mini::Function<int()> source(ThrowOnCopy{71});
    mini::Function<int()> destination([] { return 29; });

    ThrowOnCopy::should_throw = true;
    bool caught = false;
    try {
        destination = source;
    } catch (const CopyFailure&) {
        caught = true;
    }
    ThrowOnCopy::should_throw = false;

    CHECK(caught);
    CHECK(source() == 71);
    CHECK(destination() == 29);
}

void test_copy_only_callable_from_lvalue() {
    CopyOnlyCallable callable{37};
    mini::Function<int(int)> original(callable);
    mini::Function<int(int)> copy(original);

    CHECK(original(5) == 42);
    CHECK(copy(5) == 42);
}
#endif

#if MINI_FUNCTION_STAGE >= 3
void test_null_function_and_member_pointers_create_empty_function() {
    using FunctionPointer = int (*)(int);
    FunctionPointer null_function = nullptr;
    mini::Function<int(int)> first(null_function);
    CHECK(!first);

    first = &add_one;
    CHECK(first(9) == 10);
    first = null_function;
    CHECK(!first);

    using MemberPointer = int MemberTarget::*;
    MemberPointer null_member = nullptr;
    mini::Function<int&(MemberTarget&)> second(null_member);
    CHECK(!second);
}

void test_member_function_and_data_member_pointers() {
    MemberTarget target;

    mini::Function<int(const MemberTarget&, int)> member_function(
        &MemberTarget::add);
    CHECK(member_function(target, 5) == 42);

    mini::Function<int&(MemberTarget&)> member_data(&MemberTarget::value);
    int& value = member_data(target);
    CHECK(&value == &target.value);
    value = 91;
    CHECK(target.value == 91);
}

void test_void_result_discards_callable_result() {
    int calls = 0;
    mini::Function<void()> function([&calls] {
        ++calls;
        return 42;
    });

    function();
    CHECK(calls == 1);
}

void test_move_only_argument_is_forwarded() {
    mini::Function<int(std::unique_ptr<int>)> consume(
        [](std::unique_ptr<int> value) { return *value; });

    auto value = std::make_unique<int>(42);
    CHECK(consume(std::move(value)) == 42);
    CHECK(value == nullptr);
}

void test_reference_wrapper_preserves_reference_semantics() {
    Counter counter{0};
    mini::Function<int()> first(std::ref(counter));
    mini::Function<int()> second(first);

    CHECK(first() == 1);
    CHECK(counter.value == 1);
    CHECK(second() == 2);
    CHECK(counter.value == 2);
}

void test_target_exception_propagates_unchanged() {
    mini::Function<void()> function([] { throw TargetFailure{}; });

    bool caught = false;
    try {
        function();
    } catch (const TargetFailure&) {
        caught = true;
    }
    CHECK(caught);
}
#endif

} // namespace

int main() {
    std::cout << "[stage 1] heap-backed callable type erasure\n";
    test_empty_state_and_bad_function_call();
    test_function_pointer_functor_and_lambda();
    test_mutable_lambda_and_void_return();

#if MINI_FUNCTION_STAGE >= 2
    std::cout << "[stage 2] copy/move value semantics\n";
    test_copy_constructor_clones_callable_state();
    test_empty_copy_and_move_constructor();
    test_move_constructor_transfers_target();
    test_copy_and_move_assignment();
    test_null_callable_assignment_and_swap();
    test_copy_assignment_has_strong_exception_guarantee();
    test_copy_only_callable_from_lvalue();
#endif

#if MINI_FUNCTION_STAGE >= 3
    std::cout << "[stage 3] constraints and callable edge cases\n";
    test_null_function_and_member_pointers_create_empty_function();
    test_member_function_and_data_member_pointers();
    test_void_result_discards_callable_result();
    test_move_only_argument_is_forwarded();
    test_reference_wrapper_preserves_reference_semantics();
    test_target_exception_propagates_unchanged();
#endif

    std::cout << "All enabled mini::Function tests passed (stage "
              << MINI_FUNCTION_STAGE << ")\n";
}
