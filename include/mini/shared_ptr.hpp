#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>


namespace mini {

template <class T>
class SharedPtr;

template <class T>
class WeakPtr;

template <class T, class... Args>
[[nodiscard]] SharedPtr<T> make_shared(Args&&... args);

namespace detail {

struct ControlBlockBase {
    std::atomic<std::size_t> strong_count{1};
    std::atomic<std::size_t> weak_count{1};
    virtual void destroy_object() noexcept = 0;
    virtual ~ControlBlockBase() = default;
};

template <class U, class Deleter>
struct PointerControlBlock final : ControlBlockBase {
    U* pointer = nullptr;
    Deleter deleter_;

    explicit PointerControlBlock(U* pointer, Deleter deleter)
        : pointer{pointer}, deleter_{deleter} {}

    void destroy_object() noexcept override {
        deleter_(pointer);
    }

};

template <class U>
struct InplaceControlBlock final : ControlBlockBase {
public:
    template <class... Args>
    explicit InplaceControlBlock(Args&&... args) {
        std::construct_at(reinterpret_cast<U*>(storage_), std::forward<Args>(args)...);
    }

    U* get()  noexcept {
        return reinterpret_cast<U*>(storage_);
    }

    void destroy_object() noexcept override {
        std::destroy_at(get());
    }

private:
    alignas(U) std::byte storage_[sizeof(U)];
};

} // namespace detail


template <class T>
class SharedPtr {
public:
    using element_type = T;

    SharedPtr() noexcept = default;


    template <class U>
    requires std::is_convertible_v<U*, T*>
    explicit SharedPtr(U* pointer)
        : SharedPtr(pointer, std::default_delete<U>{}) {}


    template <class U, class D>
    requires std::is_convertible_v<U*, T*>
    explicit SharedPtr(U* pointer, D deleter) {
        try {
            pointer_ = pointer;
            control_ = new detail::PointerControlBlock<U, D>(pointer, deleter);
        } catch (...) {
            deleter(pointer);
            throw;
        }
    }

    SharedPtr(const SharedPtr& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        if (control_ != nullptr) control_->strong_count.fetch_add(1, std::memory_order_relaxed);
    }

    SharedPtr(SharedPtr&& other) noexcept {
        std::swap(pointer_, other.pointer_);
        std::swap(control_, other.control_);
    }

    template <class U>
    requires std::is_convertible_v<U*, T*>
    SharedPtr(const SharedPtr<U>& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        if (control_ != nullptr) control_->strong_count.fetch_add(1, std::memory_order_relaxed);
    }

    template <class U>
    requires std::is_convertible_v<U*, T*>
    SharedPtr(SharedPtr<U>&& other) noexcept
    : pointer_{other.pointer_}, control_{other.control_}
    {
        other.pointer_ = nullptr;
        other.control_ = nullptr;
    }

    template<class U>
    SharedPtr(const SharedPtr<U>& owner, T* pointer) noexcept
    : pointer_{pointer}, control_{owner.control_}
    {
        if (control_ != nullptr) control_ -> strong_count.fetch_add(1, std::memory_order_relaxed);
    }



    template<class U>
    SharedPtr(SharedPtr<U>&& owner, T* pointer) noexcept {
        pointer_ = pointer;
        control_ = owner.control_;
        owner.control_ = nullptr;
        owner.pointer_ = nullptr;
    }

    SharedPtr& operator=(SharedPtr other) noexcept {
        swap(other);
        return *this;
    }

    template <class U, class... Args>
    friend SharedPtr<U> make_shared(Args&&... args);

    ~SharedPtr() {
        release();
    }

    [[nodiscard]] T* get() const noexcept {
        return pointer_;
    }

    [[nodiscard]] std::size_t use_count() const noexcept {
        return (control_ == nullptr ? 0 : control_->strong_count.load(std::memory_order_relaxed));
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return (pointer_ != nullptr);
    }

    T& operator*() const noexcept {
        return *pointer_;
    }

    T* operator->() const noexcept {
        return pointer_;
    }

    void reset() noexcept {
        release();
    }

    void swap(SharedPtr& other) noexcept {
        std::swap(pointer_, other.pointer_);
        std::swap(control_, other.control_);
    }

private:
    template <class>
    friend class SharedPtr;

    template <class>
    friend class WeakPtr;

    void release() noexcept {
        if (control_ == nullptr) {
            pointer_ = nullptr;
            return;
        }
        if (control_ -> strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            control_->destroy_object();
            if (control_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete control_;
            }
        }
        pointer_ = nullptr;
        control_ = nullptr;
    }

    T* pointer_ = nullptr;
    detail::ControlBlockBase* control_ = nullptr;
};

template <class T, class... Args>
[[nodiscard]] SharedPtr<T> make_shared(Args&&... args)
{
    auto result = SharedPtr<T>{};
    auto* block = new detail::InplaceControlBlock<T>(std::forward<Args>(args)...);
    result.control_ = block;
    result.pointer_ = block->get();
    return result;
}

template<class To, class From>
SharedPtr<To> static_pointer_cast(
    const SharedPtr<From>& owner
) noexcept {
    To* pointer = static_cast<To*>(owner.get());
    return SharedPtr<To>{owner, pointer};
}

template<class To, class From>
SharedPtr<To> dynamic_pointer_cast(
    const SharedPtr<From>& owner
) noexcept {
    To* pointer = dynamic_cast<To*>(owner.get());
    if (pointer == nullptr) {
        return SharedPtr<To>{};
    }
    return SharedPtr<To>{owner, pointer};
}
} // namespace mini
