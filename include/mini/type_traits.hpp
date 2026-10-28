#pragma once

#include <cstddef>
#include <concepts>
#include <utility>

namespace mini {

// Stage 1: compile-time values and predicates ---------------------------------

template <class T, T Value>
struct integral_constant {
    using value_type = T;
    using type = integral_constant<T, Value>;

    static constexpr value_type value = Value;

    constexpr operator value_type() const noexcept {
        return value;
    }

    constexpr value_type operator()() const noexcept {
        return value;
    }
};

template <bool Value>
using bool_constant = integral_constant<bool, Value>;

using true_type = bool_constant<true>;
using false_type = bool_constant<false>;

template <class T, class U>
struct is_same : false_type {};

template <class T>
struct is_same<T, T> : true_type {};

template <class T, class U>
inline constexpr bool is_same_v = is_same<T, U>::value;

// Stage 2: type transformations ------------------------------------------------

template <class T>
struct is_pointer : false_type {};

template <class T>
struct is_pointer<T*> : true_type {};

template <class T>
struct remove_pointer {
    using type = T;
};

template <class T>
struct remove_pointer<T*> {
    using type = T;
};

template <class T>
struct remove_all_pointers {
    using type = T;
};

template <class T>
struct remove_all_pointers<T*> {
    using type = typename remove_all_pointers<T>::type;
};

template <class T>
struct remove_const {
    using type = T;
};

template <class T>
struct remove_const<const T> {
    using type = T;
};

template <class T>
using remove_const_t = typename remove_const<T>::type;

template <class T>
struct remove_volatile {
    using type = T;
};

template <class T>
struct remove_volatile<volatile T> {
    using type = T;
};

template <class T>
using remove_volatile_t = typename remove_volatile<T>::type;

template <class T>
struct remove_cv {
    using type = remove_volatile_t<remove_const_t<T>>;
};

template <class T>
using remove_cv_t = typename remove_cv<T>::type;

template <class T>
struct remove_reference {
    using type = T;
};

template <class T>
struct remove_reference<T&> {
    using type = T;
};

template <class T>
struct remove_reference<T&&> {
    using type = T;
};

template <class T>
using remove_reference_t = typename remove_reference<T>::type;

template <class T>
struct remove_cvref {
    using type = remove_cv_t<remove_reference_t<T>>;
};

template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;

// Stage 3: compile-time selection and SFINAE -----------------------------------

template <bool Condition, class TrueType, class FalseType>
struct conditional {
    using type = FalseType;
};

template <class TrueType, class FalseType>
struct conditional<true, TrueType, FalseType> {
    using type = TrueType;
};

template <bool Condition, class TrueType, class FalseType>
using conditional_t = typename conditional<Condition, TrueType, FalseType>::type;

template <bool Condition, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
    using type = T;
};

template <bool Condition, class T = void>
using enable_if_t = typename enable_if<Condition, T>::type;

// Stage 4: detecting valid expressions ----------------------------------------
//
// Write these yourself below (an alias template cannot be declared without a
// definition, so there are no declarations to fill in). For this stage only,
// you may include <utility> for std::declval and <concepts> for
// std::convertible_to. See docs/type_traits.md.
//
//   void_t<Ts...>                     alias for void
//   has_size<T> / has_size_v<T>       detection idiom: `t.size()` is valid for
//                                     a non-const lvalue `t` of type T
//   is_equality_comparable<T> / _v    detection idiom: `a == b` is valid for
//                                     const lvalues `a`, `b` of type T
//   concept Sized<T>                  requires-expression: for a const T `t`,
//                                     `t.size()` is convertible to std::size_t
//   concept EqualityComparable<T>     requires-expression: for const T `a`, `b`,
//                                     `a == b` is convertible to bool
template <class...>
using void_t = void;

template <class T, class = void>
struct has_size : false_type {};

template <class T>
struct has_size<T, void_t<decltype(std::declval<T&>().size())>> : true_type {};

template <class T>
inline constexpr bool has_size_v = has_size<T>::value;

template <class T, class = void>
struct is_equality_comparable : false_type {};

template <class T>
struct is_equality_comparable<
    T, void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>> : true_type {};

template <class T>
inline constexpr bool is_equality_comparable_v = is_equality_comparable<T>::value;

template <class T>
concept EqualityComparable = requires(const T& a, const T& b) {
    { a == b } -> std::convertible_to<bool>;
};

template <class T>
concept Sized = requires(const T& value) {
    { value.size() } -> std::convertible_to<std::size_t>;
};

} // namespace mini
