#pragma once

#include <cassert>
#include <cstddef>
#include <new>

#include "mini/forward.hpp"
#include "mini/move.hpp"
namespace mini {

template <typename T>
class Optional {
public:
    Optional() noexcept = default;

    ~Optional() {
        reset();
    }

    Optional(const T& value) {
        emplace(value);
    }
    Optional(T&& value) {
        emplace(mini::move(value));
    }

    Optional(const Optional& other) {
        if (other.has_value() == false) {
            reset();
        } else { 
            emplace(other.value());  
        }
    }
    Optional& operator=(const Optional& other) {
        if (this == &other) {
            return *this;
        }
        if (other.has_value() == false) {
            reset();
        } 
        else if (this -> has_value() == false) {
            emplace(other.value());
        }
        else this -> value() = other.value();
        return *this;
    }

    Optional(Optional&& other) noexcept {
        if (other.has_value() == false) {
            reset();
        } else { 
            emplace(mini::move(other.value()));
        }
    }
    Optional& operator=(Optional&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (other.has_value() == false) {
            reset();
        } 
        else if (this -> has_value() == false) {
            emplace(mini::move(other.value()));
        }
        else this -> value() = mini::move(other.value());
        return *this;
    }

    bool has_value() const noexcept {
        return has_value_;
    }

    explicit operator bool() const noexcept {
        return has_value_;
    }

    T& value() {
        return reinterpret_cast<T&>(storage_); 
    }
    const T& value() const {
        return reinterpret_cast<const T&>(storage_);
    }

    T& operator*() {
        return reinterpret_cast<T&>(storage_); 
    }

    const T& operator*() const {
        return reinterpret_cast<const T&>(storage_);
    }

    T* operator->() {
        return reinterpret_cast<T*>(storage_); 
    }


    const T* operator->() const {
        return reinterpret_cast<const T*>(storage_); 
    }

    template <typename... Args>
    T& emplace(Args&&... args) {
        reset();
        T* ptr = new (storage_) T(mini::forward<Args>(args)...);
        this -> has_value_ = true;
        return *ptr;
    }

    void reset() {
        if (has_value_ == false) {
            return;
        }
        this -> ptr() -> ~T();
        this -> has_value_ = false;
    }

private:
    // helper functions
    T* ptr() {
        return reinterpret_cast<T*>(storage_);
    }

private:
    alignas(T) std::byte storage_[sizeof(T)];
    bool has_value_ = false;
};

} // namespace mini