#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>
#include <algorithm>


namespace mini {

// Training exercise: a dynamically growing deque backed by a circular buffer.
//
// Define every member in this header. The checkpoint contract and build
// commands are in README.md.
template <class T>
class RingDeque {
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    // Checkpoint 1: raw lifetime, both ends, and wrap-around.
    explicit RingDeque(size_type initial_capacity = 8) {
        data_ = allocate_raw(initial_capacity);
        capacity_ = initial_capacity;
        head_ = 0;
        size_ = 0;
    }

    ~RingDeque() {
        clear();
        deallocate_raw(data_);
        head_ = 0;
        size_ = 0;
    }

    [[nodiscard]] bool empty() const noexcept  {
        return size_ == 0;
    }

    [[nodiscard]] size_type size() const noexcept {
        return size_;
    }

    [[nodiscard]] size_type capacity() const noexcept {
        return capacity_;
    }

    reference front() {
        return *(data_ + head_);
    }
    
    const_reference front() const {
        return *(data_ + head_);
    }

    reference back() {
        assert(!empty());
        return data_[physical_index(size_ - 1)];
    }

    const_reference back() const {
        assert(!empty());
        return data_[physical_index(size_ - 1)];
    }

    reference operator[](size_type index) {
        assert(index < size_);
        return data_[physical_index(index)];
    }
    const_reference operator[](size_type index) const {
        assert(index < size_);
        return data_[physical_index(index)];
    }

    template <class... Args>
    reference emplace_front(Args&&... args) {
        grow_for_insert();
        if (head_ == 0) head_ = capacity_ - 1;
        else head_--;
        ::new (slot(0)) T(std::forward<Args>(args)...);
        size_++;
        return *slot(0);
    }

    template <class... Args>
    reference emplace_back(Args&&... args) {
        grow_for_insert();
        ::new (slot(size_)) T(std::forward<Args>(args)...);
        size_++;
        return *slot(size_ - 1);
    }

    void push_front(const T& value) {
        emplace_front(value);
    }

    void push_front(T&& value) {
        emplace_front(std::move(value));
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    void pop_front() {
        assert(!empty());
        data_[head_].~T();
        head_++;
        head_ %= capacity_;
        size_--;
    }

    void pop_back() {
        assert(!empty());
        back().~T();
        size_--;
    }

    void clear() noexcept {
        while(size_) {
            pop_back();
        }
    }

    // Checkpoint 2: automatic growth and relocation in logical order.
    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_) return;
        reallocate(new_capacity);
    }

    // Checkpoint 3: value semantics.
    RingDeque(const RingDeque& other) 
    : RingDeque{}
    {
        for (int i = 0; i < other.size(); i++) {
            push_back(other[i]);
        }
    }

    RingDeque& operator=(const RingDeque& other) {
        clear();
        for (int i = 0; i < other.size(); i++) {
            push_back(other[i]);
        }
    }

    RingDeque(RingDeque&& other) noexcept
    : RingDeque{}
    {
        data_ = other.data_;
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        other.data_ = nullptr;
    }

    RingDeque& operator=(RingDeque&& other) noexcept 
    {
        clear();
        data_ = other.data_;
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        other.data_ = nullptr;
    }

    void swap(RingDeque& other) noexcept {
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(data_, other.data_);
    }

private:
    static constexpr size_type default_capacity = 8;

    // Logical element i lives at data_[physical_index(i)]. Only call this when
    // capacity_ is non-zero.
    [[nodiscard]] size_type physical_index(size_type logical_index) const noexcept {
        assert(capacity_ > 0);
        return (head_ + logical_index) % capacity_;
    }

    [[nodiscard]] T* slot(size_type logical_index) noexcept {
        assert(capacity_ > 0);
        return data_ + physical_index(logical_index);
    }   

    [[nodiscard]] const T* slot(size_type logical_index) const noexcept {
        assert(capacity_ > 0);
        return data_ + physical_index(logical_index);
    }

    static T* allocate_raw(size_type capacity) {
        void* raw = ::operator new(sizeof(T) * capacity, std::align_val_t(alignof(T)));
        T* data = static_cast<T*>(raw);
        return data;
    }

    static void deallocate_raw(T* data) noexcept {
        ::operator delete(data, std::align_val_t(alignof(T)));
    }

    void grow_for_insert() {
        if (size_ < capacity_) {
            return;
        }
        const size_type new_capacity = std::max(size_type{1}, capacity_ * 2);
        reallocate(new_capacity);
    }

    void reallocate(size_type new_capacity) {
        if (new_capacity <= capacity_) return;
        T* new_data = allocate_raw(new_capacity);
        for (int i = 0; i < size_; i++) {
            ::new (new_data + i) T(std::move(data_[physical_index(i)]));
            data_[physical_index(i)].~T();
        }

        deallocate_raw(data_);

        data_ = new_data;
        head_ = 0;
        capacity_ = new_capacity;
    }

    T* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
    size_type head_ = 0;
};

} // namespace mini
