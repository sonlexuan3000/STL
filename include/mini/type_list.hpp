#pragma once

#include <cstddef>

#include "mini/type_traits.hpp"

namespace mini {

// A compile-time list of types. It holds no data; it only carries `Ts...`
// so that metafunctions can pattern-match on it.
template <class... Ts>
struct type_list {};

// Stage 1: recursion over type lists -------------------------------------------
//
// Implement each metafunction with a primary template plus specializations.
// Do not use sizeof...(Ts) or anything from <type_traits> / <tuple>.

// Number of types in the list.
template <class List>
struct length;

template <>
struct length<type_list<>> : integral_constant<std::size_t, 0> {};

template <class Head, class... Tail>
struct length<type_list<Head, Tail...>>
    : integral_constant<std::size_t, length<type_list<Tail...>>::value + 1> {};

template <class List>
inline constexpr std::size_t length_v = length<List>::value;

// First type of a non-empty list. An empty list has no `type`.
template <class List>
struct front;

template <>
struct front<type_list<>> {};

template <class Head, class... Tail>
struct front<type_list<Head, Tail...>> {
    using type = Head;
};

template <class List>
using front_t = typename front<List>::type;

// The list with T added at the front.
template <class T, class List>
struct push_front;

template <class Head, class... Tail>
struct push_front<Head, type_list<Tail...>> {
    using type = type_list<Head, Tail...>;
};

template <class T, class List>
using push_front_t = typename push_front<T, List>::type;

// Whether T appears in the list (exact match: `int` is not `const int`).
template <class T, class List>
struct contains;

template <class T>
struct contains<T, type_list<>> : false_type {};

template <class T, class Head, class... Tail>
struct contains<T, type_list<Head, Tail...>>
    : bool_constant<is_same_v<T, Head> || contains<T, type_list<Tail...>>::value> {};

template <class T, class List>
inline constexpr bool contains_v = contains<T, List>::value;

// The type at zero-based index I.
template <std::size_t I, class List>
struct at;

template <class Head, class... Tail>
struct at<0, type_list<Head, Tail...>> {
    using type = Head;
};

template <std::size_t I, class Head, class... Tail>
struct at<I, type_list<Head, Tail...>> {
    using type = typename at<I - 1, type_list<Tail...>>::type;
};

template <std::size_t I, class List>
using at_t = typename at<I, List>::type;

// Stage 3: search ----------------------------------------------------------------

// Zero-based index of the first occurrence of T. Only required to work when
// T is present.
template <class T, class List>
struct index_of;

template <class T, class Head, class... Tail>
struct index_of<T, type_list<Head, Tail...>>
    : integral_constant<std::size_t, 1 + index_of<T, type_list<Tail...>>::value> {};

template <class T, class... Tail>
struct index_of<T, type_list<T, Tail...>> : integral_constant<std::size_t, 0> {};

template <class T, class List>
inline constexpr std::size_t index_of_v = index_of<T, List>::value;

// Number of times T appears in the list.
template <class T, class List>
struct count;

template <class T>
struct count<T, type_list<>> : integral_constant<std::size_t, 0> {};

template <class T, class Head, class... Tail>
struct count<T, type_list<Head, Tail...>>
    : integral_constant<std::size_t,
                        (is_same_v<T, Head> ? 1 : 0) + count<T, type_list<Tail...>>::value> {};

template <class T, class List>
inline constexpr std::size_t count_v = count<T, List>::value;

} // namespace mini
