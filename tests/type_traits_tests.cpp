#include "mini/type_traits.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#ifndef MINI_TYPE_TRAITS_STAGE
#define MINI_TYPE_TRAITS_STAGE 0
#endif

static_assert(MINI_TYPE_TRAITS_STAGE >= 0 && MINI_TYPE_TRAITS_STAGE <= 4,
              "MINI_TYPE_TRAITS_STAGE must be between 0 and 4");

namespace {

#if MINI_TYPE_TRAITS_STAGE >= 1

using Answer = mini::integral_constant<int, 42>;

static_assert(Answer::value == 42);
static_assert(std::is_same_v<Answer::value_type, int>);
static_assert(std::is_same_v<Answer::type, Answer>);
static_assert(static_cast<int>(Answer{}) == 42);
static_assert(Answer{}() == 42);
static_assert(noexcept(static_cast<int>(Answer{})));
static_assert(noexcept(Answer{}()));

static_assert(mini::true_type::value);
static_assert(!mini::false_type::value);
static_assert(mini::is_same_v<int, int>);
static_assert(!mini::is_same_v<int, const int>);
static_assert(!mini::is_same_v<int, long>);

#endif

#if MINI_TYPE_TRAITS_STAGE >= 2

static_assert(std::is_same_v<mini::remove_const_t<const int>, int>);
static_assert(std::is_same_v<mini::remove_const_t<const int*>, const int*>);
static_assert(std::is_same_v<mini::remove_volatile_t<volatile int>, int>);
static_assert(std::is_same_v<mini::remove_cv_t<const volatile int>, int>);

static_assert(std::is_same_v<mini::remove_reference_t<int&>, int>);
static_assert(std::is_same_v<mini::remove_reference_t<int&&>, int>);
static_assert(std::is_same_v<mini::remove_reference_t<int>, int>);
static_assert(std::is_same_v<mini::remove_reference_t<int (&)[3]>, int[3]>);

static_assert(std::is_same_v<mini::remove_cvref_t<const volatile int&&>, int>);
static_assert(std::is_same_v<mini::remove_cvref_t<const int (&)[3]>, int[3]>);

#endif

#if MINI_TYPE_TRAITS_STAGE >= 3

static_assert(std::is_same_v<mini::conditional_t<true, int, double>, int>);
static_assert(std::is_same_v<mini::conditional_t<false, int, double>, double>);
static_assert(std::is_same_v<mini::enable_if_t<true>, void>);
static_assert(std::is_same_v<mini::enable_if_t<true, long>, long>);

template <class T, class = mini::enable_if_t<mini::is_same_v<T, int>>>
constexpr bool accepts_only_int(T) {
    return true;
}

template <class T>
concept HasEnabledType = requires {
    typename mini::enable_if<mini::is_same_v<T, int>>::type;
};

static_assert(accepts_only_int(7));
static_assert(HasEnabledType<int>);
static_assert(!HasEnabledType<double>);

#endif

#if MINI_TYPE_TRAITS_STAGE >= 4

static_assert(std::is_same_v<mini::void_t<>, void>);
static_assert(std::is_same_v<mini::void_t<int>, void>);
static_assert(std::is_same_v<mini::void_t<int, double, const char*>, void>);

struct NoMembers {};

struct ConstSize {
    std::size_t size() const { return 1; }
};

struct NonConstSize {
    std::size_t size() { return 1; }
};

struct StringSize {
    std::string size() const { return "one"; }
};

struct DefaultedEquality {
    int value;
    friend bool operator==(const DefaultedEquality&, const DefaultedEquality&) = default;
};

struct VoidEquality {
    friend void operator==(const VoidEquality&, const VoidEquality&) {}
};

struct MutableRefEquality {
    // Only callable with non-const operands.
    friend bool operator==(MutableRefEquality&, MutableRefEquality&) { return true; }
};

struct NonConstEquality {
    bool operator==(const NonConstEquality&);  // not callable on const; only used unevaluated
};

// Detection idiom: has_size
static_assert(mini::has_size_v<std::string>);
static_assert(mini::has_size_v<std::vector<int>>);
static_assert(mini::has_size_v<const std::string>);
static_assert(mini::has_size_v<ConstSize>);
static_assert(mini::has_size_v<NonConstSize>);   // non-const lvalue can call it
static_assert(mini::has_size_v<StringSize>);     // only validity is checked
static_assert(!mini::has_size_v<int>);
static_assert(!mini::has_size_v<int*>);
static_assert(!mini::has_size_v<NoMembers>);
static_assert(std::is_base_of_v<mini::true_type, mini::has_size<std::string>>);
static_assert(std::is_base_of_v<mini::false_type, mini::has_size<int>>);
static_assert(std::is_same_v<decltype(mini::has_size_v<int>), const bool>);

// Detection idiom: is_equality_comparable
static_assert(mini::is_equality_comparable_v<int>);
static_assert(mini::is_equality_comparable_v<std::string>);
static_assert(mini::is_equality_comparable_v<DefaultedEquality>);
static_assert(mini::is_equality_comparable_v<VoidEquality>);   // only validity is checked
static_assert(!mini::is_equality_comparable_v<NoMembers>);
static_assert(!mini::is_equality_comparable_v<NonConstEquality>);
static_assert(!mini::is_equality_comparable_v<MutableRefEquality>);  // operands are const
static_assert(std::is_base_of_v<mini::true_type, mini::is_equality_comparable<int>>);
static_assert(std::is_base_of_v<mini::false_type, mini::is_equality_comparable<NoMembers>>);

// Concepts: they also check constness and result types
static_assert(mini::Sized<std::string>);
static_assert(mini::Sized<std::vector<int>>);
static_assert(mini::Sized<ConstSize>);
static_assert(!mini::Sized<NonConstSize>);       // t is const inside the concept
static_assert(!mini::Sized<StringSize>);         // std::string is not convertible to size_t
static_assert(!mini::Sized<int>);
static_assert(!mini::Sized<NoMembers>);

static_assert(mini::EqualityComparable<int>);
static_assert(mini::EqualityComparable<std::string>);
static_assert(mini::EqualityComparable<DefaultedEquality>);
static_assert(!mini::EqualityComparable<VoidEquality>);  // void is not convertible to bool
static_assert(!mini::EqualityComparable<NoMembers>);
static_assert(!mini::EqualityComparable<NonConstEquality>);
static_assert(!mini::EqualityComparable<MutableRefEquality>);

// Subsumption: the overload with the stricter concept wins when both match.
template <class T>
concept Reservable = mini::Sized<T> && requires(T& t) { t.reserve(std::size_t{}); };

template <mini::Sized T>
constexpr int prepare(T&) {
    return 1;
}

template <Reservable T>
constexpr int prepare(T&) {
    return 2;
}

template <class T>
concept CanPrepare = requires(T& t) { prepare(t); };

static_assert(!CanPrepare<int>);
static_assert(!CanPrepare<NonConstSize>);

void stage_4() {
    std::string text;
    std::vector<int> numbers;
    ConstSize fixed;
    VoidEquality{} == VoidEquality{};  // valid, but yields void
    MutableRefEquality left;
    MutableRefEquality right;
    if (!(left == right)) {
        std::abort();
    }
    if (!(DefaultedEquality{1} == DefaultedEquality{1}) || DefaultedEquality{1} == DefaultedEquality{2}) {
        std::cerr << "stage 4: defaulted operator== misbehaves\n";
        std::abort();
    }
    if (prepare(text) != 2 || prepare(numbers) != 2 || prepare(fixed) != 1) {
        std::cerr << "stage 4: overload resolution by subsumption picked the wrong overload\n";
        std::abort();
    }
}

#endif

} // namespace

int main() {
#if MINI_TYPE_TRAITS_STAGE >= 4
    stage_4();
#endif
    std::cout << "all enabled type-traits stages passed\n";
}
