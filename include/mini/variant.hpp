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

namespace detail {

template <class... Ts>
struct max_size;

template <class Head, class... Tail>
struct max_size<Head, Tail...> {
    static constexpr std::size_t value =
        sizeof(Head) > max_size<Tail...>::value ? sizeof(Head) : max_size<Tail...>::value;

    static constexpr std::size_t align =
        alignof(Head) > max_size<Tail...>::align ? alignof(Head) : max_size<Tail...>::align;
};

template <>
struct max_size<> {
    static constexpr std::size_t value = 0;
    static constexpr std::size_t align = 0;
};

template <class T, class = void>
struct is_nothrow_move_constructible : false_type {};

template <class T>
struct is_nothrow_move_constructible<T, void_t<decltype(T(std::declval<T&&>()))>>
    : bool_constant<noexcept(T(std::declval<T&&>()))> {};

template <class T, class = void>
struct is_nothrow_move_assignable : false_type {};

template <class T>
struct is_nothrow_move_assignable<
    T, void_t<decltype(std::declval<T&>() = std::declval<T&&>())>>
    : bool_constant<noexcept(std::declval<T&>() = std::declval<T&&>())> {};

template <class... Ts>
inline constexpr bool all_nothrow_move_constructible_v =
    (is_nothrow_move_constructible<Ts>::value && ...);

template <class... Ts>
inline constexpr bool all_nothrow_move_assignable_v =
    (is_nothrow_move_assignable<Ts>::value && ...);

} // namespace detail

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
class Variant {
public:
    static_assert(length_v<type_list<Ts...>> > 0, "mini::Variant requires an alternative");

    template <std::size_t I>
    using alt = at_t<I, type_list<Ts...>>;

    Variant() {
        using T = alt<0>;
        ::new (static_cast<void*>(storage_)) T();
    }

    template <class T,
              class = mini::enable_if_t<
                  mini::count_v<remove_cvref_t<T>, type_list<Ts...>> == 1>>
    Variant(T&& value) {
        using U = remove_cvref_t<T>;
        ::new (static_cast<void*>(storage_)) U(std::forward<T>(value));
        index_ = index_of_v<U, type_list<Ts...>>;
    }

    Variant(const Variant& other) {
        copy_from(other);
    }

    Variant(Variant&& other) noexcept(detail::all_nothrow_move_constructible_v<Ts...>) {
        move_from(other);
    }

    Variant& operator=(const Variant& other) {
        if (&other == this) {
            return *this;
        }
        if (other.index() == index_) {
            copy_assign_same(other);
        } else {
            destroy();
            copy_from(other);
        }
        return *this;
    }

    Variant& operator=(Variant&& other) noexcept(
        detail::all_nothrow_move_constructible_v<Ts...> &&
        detail::all_nothrow_move_assignable_v<Ts...>) {
        if (&other == this) {
            return *this;
        }
        if (other.index() == index_) {
            move_assign_same(other);
        } else {
            destroy();
            move_from(other);
        }
        return *this;
    }

    template <std::size_t I, class... Args>
    alt<I>& emplace(Args&&... args) {
        destroy();
        ::new (static_cast<void*>(storage_)) alt<I>(std::forward<Args>(args)...);
        index_ = I;
        return *ptr<I>();
    }

    template <class T, class... Args>
    T& emplace(Args&&... args) {
        using U = remove_cvref_t<T>;
        static_assert(count_v<U, type_list<Ts...>> == 1,
                      "mini::Variant::emplace<T>: T must appear exactly once");
        return emplace<index_of_v<U, type_list<Ts...>>>(std::forward<Args>(args)...);
    }

    template <class T,
              class = mini::enable_if_t<
                  mini::count_v<remove_cvref_t<T>, type_list<Ts...>> == 1>>
    Variant& operator=(T&& value) {
        using U = remove_cvref_t<T>;
        if (index_of_v<U, type_list<Ts...>> == index_) {
            *ptr<index_of_v<U, type_list<Ts...>>>() = std::forward<T>(value);
            return *this;
        }

        destroy();
        ::new (static_cast<void*>(storage_)) U(std::forward<T>(value));
        index_ = index_of_v<U, type_list<Ts...>>;
        return *this;
    }

    ~Variant() {
        destroy();
    }

    std::size_t index() const noexcept {
        return index_;
    }

    template <std::size_t I>
    alt<I>* ptr() noexcept {
        return std::launder(reinterpret_cast<alt<I>*>(storage_));
    }

    template <std::size_t I>
    const alt<I>* ptr() const noexcept {
        return std::launder(reinterpret_cast<const alt<I>*>(storage_));
    }

private:
    template <std::size_t I = 0>
    void destroy() noexcept {
        if constexpr (I < length_v<type_list<Ts...>>) {
            if (I == index_) {
                ptr<I>()->~alt<I>();
            } else {
                destroy<I + 1>();
            }
        }
    }

    template <std::size_t I = 0>
    void copy_from(const Variant& other) {
        if constexpr (I < length_v<type_list<Ts...>>) {
            if (I == other.index()) {
                ::new (static_cast<void*>(storage_)) alt<I>(*other.ptr<I>());
                index_ = I;
            } else {
                copy_from<I + 1>(other);
            }
        }
    }

    template <std::size_t I = 0>
    void move_from(Variant& other) noexcept(detail::all_nothrow_move_constructible_v<Ts...>) {
        if constexpr (I < length_v<type_list<Ts...>>) {
            if (I == other.index()) {
                ::new (static_cast<void*>(storage_)) alt<I>(std::move(*other.ptr<I>()));
                index_ = I;
            } else {
                move_from<I + 1>(other);
            }
        }
    }

    template <std::size_t I = 0>
    void copy_assign_same(const Variant& other) {
        if constexpr (I < length_v<type_list<Ts...>>) {
            if (I == index_) {
                *ptr<I>() = *other.ptr<I>();
                index_ = I;
            } else {
                copy_assign_same<I + 1>(other);
            }
        }
    }

    template <std::size_t I = 0>
    void move_assign_same(Variant& other) noexcept(
        detail::all_nothrow_move_assignable_v<Ts...>) {
        if constexpr (I < length_v<type_list<Ts...>>) {
            if (I == index_) {
                *ptr<I>() = std::move(*other.ptr<I>());
                index_ = I;
            } else {
                move_assign_same<I + 1>(other);
            }
        }
    }

    alignas(detail::max_size<Ts...>::align) std::byte storage_[detail::max_size<Ts...>::value];
    std::size_t index_{};
};

// Number of alternatives of a Variant type.
template <class V>
struct variant_size;

template <class... Ts>
struct variant_size<Variant<Ts...>> : length<type_list<Ts...>> {};

template <class V>
inline constexpr std::size_t variant_size_v = variant_size<V>::value;

// Type of alternative I of a Variant type.
template <std::size_t I, class V>
struct variant_alternative;

template <std::size_t I, class... Ts>
struct variant_alternative<I, Variant<Ts...>> : at<I, type_list<Ts...>> {};

template <std::size_t I, class V>
using variant_alternative_t = typename variant_alternative<I, V>::type;

// Whether the variant currently holds T (T must appear exactly once).
template <class T, class... Ts>
bool holds_alternative(const Variant<Ts...>& variant) noexcept {
    static_assert(count_v<T, type_list<Ts...>> == 1);
    return index_of_v<T, type_list<Ts...>> == variant.index();
}

// The active alternative. Throws bad_variant_access if the variant holds
// something else.
template <std::size_t I, class... Ts>
variant_alternative_t<I, Variant<Ts...>>& get(Variant<Ts...>& variant) {
    if (I != variant.index()) {
        throw bad_variant_access{};
    }
    return *variant.template ptr<I>();
}

template <std::size_t I, class... Ts>
const variant_alternative_t<I, Variant<Ts...>>& get(const Variant<Ts...>& variant) {
    if (I != variant.index()) {
        throw bad_variant_access{};
    }
    return *variant.template ptr<I>();
}

template <class T, class... Ts>
T& get(Variant<Ts...>& variant) {
    static_assert(count_v<T, type_list<Ts...>> == 1,
                  "mini::get<T>: T must appear exactly once in the variant");
    return get<index_of_v<T, type_list<Ts...>>, Ts...>(variant);
}

template <class T, class... Ts>
const T& get(const Variant<Ts...>& variant) {
    static_assert(count_v<T, type_list<Ts...>> == 1,
                  "mini::get<T>: T must appear exactly once in the variant");
    return get<index_of_v<T, type_list<Ts...>>, Ts...>(variant);
}

// Pointer to the alternative T if it is active, otherwise nullptr (also for a
// null `variant`).
template <class T, class... Ts>
T* get_if(Variant<Ts...>* variant) noexcept {
    if (variant == nullptr || !holds_alternative<T, Ts...>(*variant)) {
        return nullptr;
    }
    return &get<T, Ts...>(*variant);
}

template <class T, class... Ts>
const T* get_if(const Variant<Ts...>* variant) noexcept {
    if (variant == nullptr || !holds_alternative<T, Ts...>(*variant)) {
        return nullptr;
    }
    return &get<T, Ts...>(*variant);
}

// Stage 3: visitation and comparison ----------------------------------------------
namespace detail {
template <std::size_t I, class Visitor, class V>
decltype(auto) call_at(Visitor& f, V& v) {
    return f(*v.template ptr<I>());
}

template <class Visitor, class V, std::size_t... Is>
decltype(auto) visit_impl(Visitor& f, V& v, std::index_sequence<Is...>) {
    using R = decltype(std::declval<Visitor&>()(*std::declval<V&>().template ptr<0>()));
    static_assert(
        (is_same_v<R,
                   decltype(std::declval<Visitor&>()(
                       *std::declval<V&>().template ptr<Is>()))> &&
         ...),
        "mini::visit: the visitor must return the same type for every alternative");
    using Fn = R (*)(Visitor&, V&);
    static constexpr Fn function_table[] = {&call_at<Is, Visitor, V>...};
    return function_table[v.index()](f, v);
}

} // namespace detail
// Calls visitor(active alternative) and returns its result. The visitor must
// return the same type for every alternative.
template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& visitor, Variant<Ts...>& variant) {
    return detail::visit_impl(visitor, variant, std::index_sequence_for<Ts...>{});
}

template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& visitor, const Variant<Ts...>& variant) {
    return detail::visit_impl(visitor, variant, std::index_sequence_for<Ts...>{});
}

// Equal when both hold the same alternative and the values compare equal.
template <std::size_t I, class... Ts>
decltype(auto) equal_at(const Variant<Ts...>& lhs, const Variant<Ts...>& rhs) {
    return *lhs.template ptr<I>() == *rhs.template ptr<I>();
}

template <class... Ts, std::size_t... Is>
decltype(auto) equal_impl(const Variant<Ts...>& lhs, const Variant<Ts...>& rhs,
                          std::index_sequence<Is...>) {
    using Fn = bool (*)(const Variant<Ts...>&, const Variant<Ts...>&);
    static constexpr Fn function_table[] = {&equal_at<Is, Ts...>...};
    return function_table[lhs.index()](lhs, rhs);
}

template <class... Ts>
bool operator==(const Variant<Ts...>& lhs, const Variant<Ts...>& rhs) {
    if (lhs.index() != rhs.index()) {
        return false;
    }
    return equal_impl(lhs, rhs, std::index_sequence_for<Ts...>{});
}

} // namespace mini
