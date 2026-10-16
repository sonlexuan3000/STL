#pragma once

#include "forward.hpp"
#include "mini/vector.hpp"

#include <cstddef>
#include <functional>
#include <utility>

namespace mini {

template <class T,
          class Container = Vector<T>,
          class Compare = std::less<T>>
class PriorityQueue {
public:
    using value_type = T;
    using container_type = Container;
    using value_compare = Compare;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    PriorityQueue()
    : container_{}, comparator_{}
    {
    }

    PriorityQueue(Compare comparator)
    : container_{}, comparator_{comparator}
    {

    }

    PriorityQueue(Compare comparator, const Container& container)
    :container_{container}, comparator_{comparator}
    {
        build_heap();
    }

    PriorityQueue(Compare comparator, Container&& container)
    : container_{std::move(container)}, comparator_{comparator}
    {
        build_heap();
    }

    bool empty() const {
        return container_.empty();
    }

    size_type size() const {
        return container_.size();
    }

    const_reference top() const {
        return container_[0];
    }

    template<class... Args>
    void emplace(Args&&... args) {
        container_.emplace_back(mini::forward<Args>(args)...);
        sift_up(size() - 1);
    }

    void push(const T& value) {
        emplace(value);
    }

    void push(T&& value) {
        emplace(std::move(value));
    }

    void pop() {
        std::swap(container_.front(), container_.back());
        container_.pop_back();
        if(!empty()) sift_down(0);
    }

    void swap(PriorityQueue& other)
        noexcept(noexcept(std::swap(container_, other.container_)) &&
                 noexcept(std::swap(comparator_, other.comparator_))) {
        std::swap(container_, other.container_);
        std::swap(comparator_, other.comparator_);
    }

private:
    void sift_up(size_type index) {
        while (index > 0) {
            const size_type parent = (index - 1) / 2;

            if (!comparator_(container_[parent], container_[index])) {
                break;
            }

            std::swap(container_[parent], container_[index]);
            index = parent;
        }
    }

    void sift_down(size_type index) {
        const size_type count = container_.size();

        while (true) {
            const size_type left = index * 2 + 1;

            if (left >= count) {
                break;
            }

            const size_type right = left + 1;
            size_type best = left;

            if (right < count &&
                comparator_(container_[best], container_[right])) {
                best = right;
            }

            if (!comparator_(container_[index], container_[best])) {
                break;
            }

            std::swap(container_[index], container_[best]);
            index = best;
        }
    }

    void build_heap() {
        for (size_type i = container_.size() / 2; i > 0; --i) {
            sift_down(i - 1);
        }
    }

    Container container_{};
    Compare comparator_{};
};

} // namespace mini
