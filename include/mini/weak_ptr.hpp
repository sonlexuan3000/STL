#pragma once

#include "mini/shared_ptr.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace mini {

template <class T>
class WeakPtr {
public:
    using element_type = T;

    WeakPtr() noexcept = default;

    WeakPtr(const WeakPtr& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        if (control_ != nullptr) {
            control_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    WeakPtr(WeakPtr&& other) noexcept
    {
        swap(other);
    }

    WeakPtr(const SharedPtr<T>& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        if (control_ != nullptr) {
            control_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    template<class U>
    requires std::is_convertible_v<U*, T*>
    WeakPtr(const SharedPtr<U>& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        if (control_ != nullptr) {
            control_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    WeakPtr& operator=(WeakPtr other) noexcept {
        swap(other);
        return *this;
    }

    ~WeakPtr() {
        release();
    }

    void reset() noexcept {
        release();
    }

    void swap(WeakPtr& other) noexcept {
        std::swap(pointer_, other.pointer_);
        std::swap(control_, other.control_);
    }

    [[nodiscard]] std::size_t use_count() const noexcept {
        if (control_ == nullptr) return 0;
        return control_->strong_count;
    }

    [[nodiscard]] bool expired() const noexcept {
        return (use_count() == 0);
    }

    [[nodiscard]] SharedPtr<T> lock() const noexcept {
        if (control_ == nullptr) {
            return SharedPtr<T>{};
        }
        std::size_t cur = control_->strong_count;
        while(cur != 0) {
            bool success = control_->strong_count.compare_exchange_weak(cur, cur + 1);
            if (success) {
                cur++;
                break;
            } else {
                cur = control_ -> strong_count;
            }
        }
        if (cur == 0) {
            return SharedPtr<T>{};

        } else {
            auto lck = SharedPtr<T>{};
            lck.pointer_ = pointer_;
            lck.control_ = control_;
            return lck;
        }
    }

    template <class U>
    requires std::is_convertible_v<U*, T*>
    WeakPtr(const WeakPtr<U>& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        if (control_ != nullptr) {
            control_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    template <class U>
    requires std::is_convertible_v<U*, T*>
    WeakPtr(WeakPtr<U>&& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        other.pointer_ = nullptr;
        other.control_ = nullptr;
    }

private:
    template <class>
    friend class WeakPtr;

    void release() noexcept {
        if (control_ == nullptr) return;
        if (control_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete control_;
        }
        pointer_ = nullptr;
        control_ = nullptr;
    }

    T* pointer_ = nullptr;
    detail::ControlBlockBase* control_ = nullptr;
};

} // namespace mini
