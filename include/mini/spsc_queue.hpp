#pragma once

#include <cstddef>
#include <atomic>
#include <new>
#include <utility>

namespace mini {

template <class T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity > 0, "SpscQueue capacity must be positive");

public:
    using value_type = T;
    using size_type = std::size_t;

    SpscQueue() = default;

    ~SpscQueue() {
        for (size_type i = head_.load(std::memory_order_relaxed); i != tail_.load(std::memory_order_acquire); i = (i + 1) % slot_count) {
            slot(i)->~T();
        }
    }

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&) = delete;
    SpscQueue& operator=(SpscQueue&&) = delete;

    [[nodiscard]] static constexpr size_type capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool full() const noexcept {
        std::size_t next_tail =
            (tail_.load(std::memory_order_relaxed) + 1) % slot_count;
        return next_tail == head_.load(std::memory_order_acquire);
    }

    bool try_push(const T& value) {
        return try_emplace(value);
    }

    bool try_push(T&& value) {
        return try_emplace(std::move(value));
    }

    template <class... Args>
    bool try_emplace(Args&&... args) {
        if (full()) return false;
        std::size_t index = tail_.load(std::memory_order_relaxed);
        ::new (raw_slot(index)) T{std::forward<Args>(args)...};
        index = (index + 1) % slot_count;
        tail_.store(index, std::memory_order_release);
        return true;
    }

    bool try_pop(T& output) {
        if (empty()) return false;
        std::size_t index = head_.load(std::memory_order_relaxed);
        output = std::move(*slot(index));
        slot(index)->~T();
        index = (index + 1) % slot_count;
        head_.store(index, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t slot_count = Capacity + 1;

    [[nodiscard]] void* raw_slot(size_type index) noexcept {
        return storage_ + index * sizeof(T);
    }

    [[nodiscard]] const void* raw_slot(size_type index) const noexcept {
        return storage_ + index * sizeof(T);
    }

    [[nodiscard]] T* slot(size_type index) noexcept {
        return std::launder(static_cast<T*>(raw_slot(index)));
    }

    [[nodiscard]] const T* slot(size_type index) const noexcept {
        return std::launder(static_cast<const T*>(raw_slot(index)));
    }

    alignas(T) std::byte storage_[slot_count * sizeof(T)];
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

} // namespace mini
