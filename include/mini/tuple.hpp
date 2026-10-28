#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "mini/type_list.hpp"
#include "mini/type_traits.hpp"

namespace mini {

// Stage 2: storage and indexed access --------------------------------------------
//
// Define Tuple below. The representation is your choice; docs/tuple.md
// describes two classic designs. Element types are object types (no
// references). Required members:
//
//   Tuple();                       // value-initializes every element
//   explicit Tuple(const Ts&...);  // copies each argument into its element
//
// Copy and move must work element by element.
template <class... Ts>
class Tuple;

template <>
class Tuple<> {};

template <class Head, class... Ts>
class Tuple<Head, Ts...> : Tuple<Ts...> {
public:
    Tuple() = default;

    explicit Tuple(const Head& head, const Ts&... tail)
        : Tuple<Ts...>(tail...), head_{head} {}

    const Head& head() const noexcept {
        return head_;
    }

    Head& head() noexcept {
        return head_;
    }

    const Tuple<Ts...>& tail() const noexcept {
        return *this;
    }

    Tuple<Ts...>& tail() noexcept {
        return *this;
    }

private:
    Head head_{};
};

// Number of elements of a Tuple type.
template <class T>
struct tuple_size;

template <class... Ts>
struct tuple_size<Tuple<Ts...>> : integral_constant<std::size_t, length_v<type_list<Ts...>>> {};

template <class T>
inline constexpr std::size_t tuple_size_v = tuple_size<T>::value;

// Type of element I of a Tuple type.
template <std::size_t I, class T>
struct tuple_element;

template <std::size_t I, class... Ts>
struct tuple_element<I, Tuple<Ts...>> : at<I, type_list<Ts...>> {};

template <std::size_t I, class T>
using tuple_element_t = typename tuple_element<I, T>::type;

// Reference to element I.
template <std::size_t I, class... Ts>
tuple_element_t<I, Tuple<Ts...>>& get(Tuple<Ts...>& tuple) noexcept {
    if constexpr (I == 0) {
        return tuple.head();
    } else {
        return get<I - 1>(tuple.tail());
    }
}

template <std::size_t I, class... Ts>
const tuple_element_t<I, Tuple<Ts...>>& get(const Tuple<Ts...>& tuple) noexcept {
    if constexpr (I == 0) {
        return tuple.head();
    } else {
        return get<I - 1>(tuple.tail());
    }
}

// Stage 3: convenience and structured bindings ------------------------------------

// Element I of an expiring tuple, as an rvalue (needed by `auto [a, b] = ...`).
template <std::size_t I, class... Ts>
tuple_element_t<I, Tuple<Ts...>>&& get(Tuple<Ts...>&& tuple) noexcept {
    return std::move(get<I>(tuple));
}

// Reference to the element whose type is T. T must appear exactly once.
template <class T, class... Ts>
T& get(Tuple<Ts...>& tuple) noexcept {
    static_assert(count_v<T, type_list<Ts...>> == 1,
                  "mini::get<T>: T must appear exactly once in the tuple");
    return get<index_of_v<T, type_list<Ts...>>>(tuple);
}

template <class T, class... Ts>
const T& get(const Tuple<Ts...>& tuple) noexcept {
    static_assert(count_v<T, type_list<Ts...>> == 1,
                  "mini::get<T>: T must appear exactly once in the tuple");
    return get<index_of_v<T, type_list<Ts...>>>(tuple);
}

// Builds a Tuple whose element types are the arguments' types without
// references and cv-qualifiers.
template <class... Args>
Tuple<remove_cvref_t<Args>...> make_tuple(Args&&... args) {
    return Tuple<remove_cvref_t<Args>...>{std::forward<Args>(args)...};
}

// Element-wise equality.
template <class... Ts>
bool operator==(const Tuple<Ts...>& lhs, const Tuple<Ts...>& rhs) {
    return (lhs.head() == rhs.head()) && (lhs.tail() == rhs.tail());
}

inline bool operator==(const Tuple<>&, const Tuple<>&) {
    return true;
}

} // namespace mini

// Stage 3: specialize std::tuple_size and std::tuple_element for mini::Tuple
// here so that `auto [a, b] = tuple;` works.
namespace std {

template <class... Ts>
struct tuple_size<mini::Tuple<Ts...>>
    : integral_constant<size_t, mini::tuple_size_v<mini::Tuple<Ts...>>> {};

template <size_t I, class... Ts>
struct tuple_element<I, mini::Tuple<Ts...>> {
    using type = mini::tuple_element_t<I, mini::Tuple<Ts...>>;
};

} // namespace std
