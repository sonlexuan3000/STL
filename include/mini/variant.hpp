#pragma once

#include <cstddef>
#include <exception>
#include <new>
#include <utility>

#include "mini/type_list.hpp"
#include "mini/type_traits.hpp"

namespace mini {

// Thrown by get<I> / get<T> when the variant holds a different alternative.
class bad_variant_access : public std::exception {
public:
    const char* what() const noexcept override { return "mini::bad_variant_access"; }
};

// Stage 1: storage, construction, destruction, and access ----------------------
//
// Define Variant below. It holds exactly one value whose type is one of Ts...,
// stored in place (no heap allocation). Ts... is non-empty. Required members:
//
//   Variant();                     // holds a value-initialized first alternative
//   Variant(T&& value);            // for any T whose type, without cv/ref, is
//                                  // exactly one alternative (appearing once);
//                                  // copies or moves value into the variant
//   ~Variant();                    // destroys the active alternative
//   std::size_t index() const noexcept;
//
// Stage 2 adds copy/move construction and assignment, emplace<I> / emplace<T>,
// and assignment from an alternative. See docs/variant.md.
template <class... Ts>
class Variant;

// Number of alternatives of a Variant type.
template <class V>
struct variant_size;

template <class V>
inline constexpr std::size_t variant_size_v = variant_size<V>::value;

// Type of alternative I of a Variant type.
template <std::size_t I, class V>
struct variant_alternative;

template <std::size_t I, class V>
using variant_alternative_t = typename variant_alternative<I, V>::type;

// Whether the variant currently holds T (T must appear exactly once).
template <class T, class... Ts>
bool holds_alternative(const Variant<Ts...>& variant) noexcept;

// The active alternative. Throws bad_variant_access if the variant holds
// something else.
template <std::size_t I, class... Ts>
variant_alternative_t<I, Variant<Ts...>>& get(Variant<Ts...>& variant);

template <std::size_t I, class... Ts>
const variant_alternative_t<I, Variant<Ts...>>& get(const Variant<Ts...>& variant);

template <class T, class... Ts>
T& get(Variant<Ts...>& variant);

template <class T, class... Ts>
const T& get(const Variant<Ts...>& variant);

// Pointer to the alternative T if it is active, otherwise nullptr (also for a
// null `variant`).
template <class T, class... Ts>
T* get_if(Variant<Ts...>* variant) noexcept;

template <class T, class... Ts>
const T* get_if(const Variant<Ts...>* variant) noexcept;

// Stage 3: visitation and comparison ----------------------------------------------

// Calls visitor(active alternative) and returns its result. The visitor must
// return the same type for every alternative.
template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& visitor, Variant<Ts...>& variant);

template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& visitor, const Variant<Ts...>& variant);

// Equal when both hold the same alternative and the values compare equal.
template <class... Ts>
bool operator==(const Variant<Ts...>& lhs, const Variant<Ts...>& rhs);

} // namespace mini
