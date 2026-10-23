#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <atomic>

namespace mini {

namespace detail {

struct ControlBlockBase {
    std::atomic<std::size_t> strong_count{1};

    virtual void destroy_object() noexcept = 0;
    virtual ~ControlBlockBase() = default;
};

template <class U>
struct PointerControlBlock final : ControlBlockBase {
    explicit PointerControlBlock(U* pointer) noexcept
        : pointer(pointer) {}

    void destroy_object() noexcept override {
        delete pointer;
    }

    U* pointer = nullptr;
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
    : pointer_{pointer}, control_{new detail::PointerControlBlock<U>(pointer)}
    {

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

    SharedPtr& operator=(SharedPtr other) noexcept {
        swap(other);
        return *this;
    }

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

    void release() noexcept {
        if (control_ == nullptr) return;
        if (control_ -> strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            control_->destroy_object();
            delete control_;
        }
        pointer_ = nullptr;
        control_ = nullptr;
    }

    T* pointer_ = nullptr;
    detail::ControlBlockBase* control_ = nullptr;
};

} // namespace mini
