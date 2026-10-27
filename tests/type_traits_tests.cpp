#include "mini/type_traits.hpp"

#include <iostream>
#include <type_traits>

#ifndef MINI_TYPE_TRAITS_STAGE
#define MINI_TYPE_TRAITS_STAGE 0
#endif

static_assert(MINI_TYPE_TRAITS_STAGE >= 0 && MINI_TYPE_TRAITS_STAGE <= 3,
              "MINI_TYPE_TRAITS_STAGE must be between 0 and 3");

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

} // namespace

int main() {
    std::cout << "all enabled type-traits stages passed\n";
}
