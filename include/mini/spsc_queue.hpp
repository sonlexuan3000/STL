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
        for (size_type i = head_.value.load(std::memory_order_relaxed); i != tail_.value.load(std::memory_order_relaxed); i = (i + 1) % slot_count) {
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
        std::size_t head = head_.value.load(std::memory_order_relaxed);
        std::size_t tail = tail_.value.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] bool full() const noexcept {
        std::size_t head = head_.value.load(std::memory_order_acquire);
        std::size_t tail = tail_.value.load(std::memory_order_relaxed);
        return head == (tail + 1) % slot_count;
    }

    bool try_push(const T& value) {
        return try_emplace(value);
    }

    bool try_push(T&& value) {
        return try_emplace(std::move(value));
    }

    template <class... Args>
    bool try_emplace(Args&&... args) {
        std::size_t tail_value = tail_.value.load(std::memory_order_relaxed);
        if ((tail_value + 1) % slot_count == tail_.cached_head) {
            tail_.cached_head = head_.value.load(std::memory_order_acquire);
            if ((tail_value + 1) % slot_count == tail_.cached_head) {
                return false;
            }
        }
        std::size_t index = tail_value;
        ::new (raw_slot(index)) T{std::forward<Args>(args)...};
        index = (index + 1) % slot_count;
        tail_.value.store(index, std::memory_order_release);
        return true;
    }

    bool try_pop(T& output) {
        std::size_t head_value = head_.value.load(std::memory_order_relaxed);
        if (head_value == head_.cached_tail) {
            head_.cached_tail = tail_.value.load(std::memory_order_acquire);
            if (head_value == head_.cached_tail) {
                return false;
            }
        }
        std::size_t index = head_value;
        output = std::move(*slot(index));
        slot(index)->~T();
        index = (index + 1) % slot_count;
        head_.value.store(index, std::memory_order_release);
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

#if defined(__APPLE__) && defined(__aarch64__)
    // Apple Silicon reports 128-byte cache lines. Most x86 targets use 64.
    static constexpr std::size_t cache_line_size = 128;
#else
    static constexpr std::size_t cache_line_size = 64;
#endif

    struct alignas(cache_line_size) ProducerState {
        std::atomic<std::size_t> value{0};
        std::size_t cached_head{0};
    };

    struct alignas(cache_line_size) ConsumerState {
        std::atomic<std::size_t> value{0};
        std::size_t cached_tail{0};
    };

    static_assert(sizeof(ProducerState) == cache_line_size);
    static_assert(sizeof(ConsumerState) == cache_line_size);

    alignas(T) std::byte storage_[slot_count * sizeof(T)];
    ProducerState tail_{0};
    ConsumerState head_{0};
};

} // namespace mini
