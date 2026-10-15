#pragma once

namespace mini {

template <typename T>
struct remove_reference {
    using type = T;
};

template <typename T>
struct remove_reference<T&> {
    using type = T;
};

template <typename T>
struct remove_reference<T&&> {
    using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T>
struct is_lvalue_reference {
    static constexpr bool value = false;
};

template <typename T>
struct is_lvalue_reference<T&> {
    static constexpr bool value = true;
};

template <typename T>
constexpr T&& forward(remove_reference_t<T>& x) noexcept {
    return static_cast<T&&>(x);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>&& x) noexcept {
    static_assert(is_lvalue_reference<T>::value == false, "Syntax Error");
    return static_cast<T&&>(x);
}

}