#pragma once

#include "mini/forward.hpp"

namespace mini {

template <typename T>
class UniquePtr {
public:
    explicit UniquePtr(T* ptr = nullptr) 
        : ptr_(ptr) {}

    ~UniquePtr() {
        delete ptr_;
    }

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept
        : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        delete ptr_;
        this -> ptr_ = other.ptr_;
        other.ptr_ = nullptr;
        return *this;
    }

    T& operator*() const {
        return *this -> ptr_;
    }
    T* operator->() const {
        return this -> ptr_;
    }

    T* get() const {
        return this -> ptr_;
    }

    T* release() {
        T* answer = ptr_;
        ptr_ = nullptr;
        return answer;
    }

    void reset(T* ptr = nullptr) {
        if (this -> ptr_ == ptr) {
            return;
        }
        delete ptr_;
        this -> ptr_ = ptr;
    }

private:
    T* ptr_ = nullptr;
};

template <typename T, typename... Args>
UniquePtr<T> make_unique(Args&&... args) {
    return UniquePtr<T> (new T(mini::forward<Args>(args)...));
}

}