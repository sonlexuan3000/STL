#pragma once

#include "mini/forward.hpp"

namespace mini {

template <typename T>
constexpr remove_reference_t<T>&& move(T&& x) noexcept {
    return static_cast<remove_reference_t<T>&&>(x);
}

}
