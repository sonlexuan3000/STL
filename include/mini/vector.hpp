#pragma once

#include <algorithm>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mini {

template <class T>
class Vector {
public:
    // --- Construction / destruction ---
    Vector() noexcept {
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    explicit Vector(std::size_t count) {
		reserve(count);

		try {
			while (size_ < count) {
				::new (data_ + size_) T{};
				++size_;
			}
		} catch (...) {
			destroy_elements();
			deallocate_raw(data_);
			data_ = nullptr;
			capacity_ = 0;
			throw;
		}
	}

	Vector(std::size_t count, const T& value) {
		reserve(count);

		try {
			while (size_ < count) {
				:: new (data_ + size_) T(value);
				++size_;
			}
		} catch (...) {
			destroy_elements();
			deallocate_raw(data_);
			data_ = nullptr;
			capacity_ = 0;
			throw;
		}
	}        // fill
    Vector(const Vector& other) {
		if (other.size_ == 0) {
			return;
		}

		data_ = allocate_raw(other.size_);
		capacity_ = other.size_;

		try {
			while (size_ < other.size_) {
				::new (static_cast<void*>(data_ + size_))
					T(other.data_[size_]);

				++size_;
			}
		} catch (...) {
			destroy_elements();
			deallocate_raw(data_);

			data_ = nullptr;
			capacity_ = 0;

			throw;
		}
	}
	                       // copy
    Vector(Vector&& other) noexcept {
        if (this == &other) {
            return;
        }
        this->destroy_elements();
        this->deallocate_raw(data_);
        this->size_ = other.size_;
        this->capacity_ = other.capacity_;
        this->data_=other.data();
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }
	                  // move
    ~Vector() {
        destroy_elements();
        deallocate_raw(data_);
    }

    // --- Assignment ---

	Vector& operator=(const Vector& other) {
		if (this == &other) {
			return *this;
		}

		Vector temp(other); 

		std::swap(data_, temp.data_);
		std::swap(size_, temp.size_);
		std::swap(capacity_, temp.capacity_);

		return *this;
	} 
	          // strong guarantee
    Vector& operator=(Vector&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->destroy_elements();
        this->deallocate_raw(data_);
        this->size_ = other.size_;
        this->capacity_ = other.capacity_;
        this->data_=other.data();
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
        return *this;
    }

    // --- Element access ---
    T& operator[](std::size_t i) {
		return data_[i];
	}

    const T& operator[](std::size_t i) const {
		return data_[i];
	}
    T& at(std::size_t i) {
		if (i >= size_) {
			throw std::out_of_range("Vector::at");
		}
		return data_[i];
	}                            // throws std::out_of_range
    const T& at(std::size_t i) const {
		if (i >= size_) {
			throw std::out_of_range("Vector::at");
		}
		return data_[i];
	}
    T& front() {
		return *data_;
	}            
	const T& front() const {
		return *data_;
	}
    T& back() {
        return data_[size_- 1];
    }
    const T& back() const {
        return data_[size_ - 1];
    }
    T* data() noexcept {
        return data_;
    }
    const T* data() const noexcept {
        return data_;
    }

    // --- Capacity ---
    bool empty() const noexcept {
		return size_ == 0;
	}
    std::size_t size() const noexcept {
		return size_;
	}
    std::size_t capacity() const noexcept {
		return capacity_;
	}
    void reserve(std::size_t new_capacity) {
		if (new_capacity <= capacity_) {
			return;
		}

		T* new_data = allocate_raw(new_capacity);
		std::size_t constructed = 0;

		try {
			for (; constructed < size_; ++constructed) {
				::new (static_cast<void*>(new_data + constructed))
					T(std::move_if_noexcept(data_[constructed]));
			}
		} catch (...) {
			while (constructed > 0) {
				--constructed;
				new_data[constructed].~T();
			}

			deallocate_raw(new_data);
			throw;
		}

		// Chỉ destroy storage cũ khi toàn bộ relocation thành công.
		for (std::size_t i = 0; i < size_; ++i) {
			data_[i].~T();
		}

		deallocate_raw(data_);

		data_ = new_data;
		capacity_ = new_capacity;
	}
	                // strong guarantee
    void shrink_to_fit() {
		if (size_ == capacity_) {
			return;
		}

		if (size_ == 0) {
			deallocate_raw(data_);
			data_ = nullptr;
			capacity_ = 0;
			return;
		}

		T* new_data = allocate_raw(size_);
		std::size_t constructed = 0;

		try {
			for (; constructed < size_; ++constructed) {
				::new (static_cast<void*>(new_data + constructed))
					T(std::move_if_noexcept(data_[constructed]));
			}
		} catch (...) {
			while (constructed > 0) {
				--constructed;
				new_data[constructed].~T();
			}

			deallocate_raw(new_data);
			throw;
		}

		for (std::size_t i = 0; i < size_; ++i) {
			data_[i].~T();
		}

		deallocate_raw(data_);
		data_ = new_data;
		capacity_ = size_;
	}

    // --- Modifiers ---
    void clear() noexcept {
		destroy_elements();
	}

    void push_back(const T& value) {
		if (size_ < capacity_) {
			::new (static_cast<void*>(data_ + size_)) T(value);
			++size_;
			return;
		}

		// Copy trước khi reserve làm reference bị invalid.
		T saved_value(value);

		reserve(std::max<std::size_t>(capacity_ * 2, 1));

		::new (static_cast<void*>(data_ + size_))
			T(std::move_if_noexcept(saved_value));

		++size_;
	}
	                   // strong guarantee
    void push_back(T&& value) {
        if (size_ == capacity_) {
            reserve(std::max(2 * capacity_, capacity_ + 1));
        }
        ::new (data_ + size_) T(std::move(value));
        size_++;
    }                           // strong guarantee (if move is noexcept)
    
    template <typename... Args>
T& emplace_back(Args&&... args) {
    if (size_ == capacity_) {
        return grow_and_emplace_back(
            std::forward<Args>(args)...
        );
    }

    void* slot = data_ + size_;

    ::new (slot) T(std::forward<Args>(args)...);

    T* result = std::launder(
        reinterpret_cast<T*>(slot)
    );

    ++size_;
    return *result;
}                 // strong guarantee
    void pop_back() {
		if (size_ > 0) {
			size_--;
			data_[size_].~T();
		}
	}

    void resize(std::size_t new_size) {
        if (new_size <= this->size_) {
            while (this->size_ > new_size) {
                --size_;
                data_[size_].~T();
            }
        } else {
            reserve(new_size);
            while(this->size_ < new_size) {
                ::new (data_ + size_) T();
                this->size_++;
            }
        }
    }

    void resize(std::size_t new_size, const T& value) {
    if (new_size <= size_) {
        while (size_ > new_size) {
            --size_;
            data_[size_].~T();
        }
        return;
    }

    const std::size_t old_size = size_;

    if (new_size > capacity_) {
        // Phải copy trước reserve vì value có thể là data_[i].
        T saved_value(value);

        reserve(new_size);

        try {
            while (size_ < new_size) {
                ::new (static_cast<void*>(data_ + size_))
                    T(saved_value);
                ++size_;
            }
        } catch (...) {
            while (size_ > old_size) {
                --size_;
                data_[size_].~T();
            }
            throw;
        }
    } else {
        // Không reallocate nên value vẫn còn hợp lệ.
        try {
            while (size_ < new_size) {
                ::new (static_cast<void*>(data_ + size_))
                    T(value);
                ++size_;
            }
        } catch (...) {
            while (size_ > old_size) {
                --size_;
                data_[size_].~T();
            }
            throw;
        }
    }
}

    // --- Iterators (raw pointer iterators are acceptable) ---
    
    T* begin() noexcept {
        return data_;
    }

    T* end() noexcept {
        return data_ + size_ ;
    }

    const T* begin() const noexcept {
        return data_;
    }
    const T* end() const noexcept {
        return data_ + size_;
    }
    const T* cbegin() const noexcept {
        return data_;
    }
    const T* cend() const noexcept {
        return data_ + size_;
    }

    void swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }


    // --- insert / erase ---
    T* insert(const T* pos, const T& value) {
		std::size_t index = static_cast<std::size_t>(pos - data_);
		push_back(value);
		if (index != size_-1) {
			for (int i = size_ - 2;; i--) {
				std::swap(data_[i], data_[i+1]);
				if (index == i) break;
			}
		}
		return (data_ + index);
	}

    T* insert(const T* pos, T&& value) {
		std::size_t index = static_cast<std::size_t>(pos - data_);
		if (size_  == capacity_) reserve(std::max<std::size_t>(1, capacity_ * 2));
		::new (data_ + size_) T(std::move(value));
		if (index != size_) {
			for (int i = size_ - 1;; i--) {
				std::swap(data_[i], data_[i+1]);
				if (index == i) break;
			}
		}
		size_++;
		return (data_ + index);
	}
    T* erase(const T* pos) {
		std::size_t index = static_cast<std::size_t>(pos - data_);

		for (std::size_t i = index; i + 1 < size_; ++i) {
			data_[i] = std::move(data_[i + 1]);
		}

		--size_;
		(*(data_ + size_)).~T();

		return data_ + index;
	}

T* erase(const T* first, const T* last) {
    if (first == last) {
        return const_cast<T*>(first);
    }

    std::size_t first_index =
        static_cast<std::size_t>(first - data_);

    std::size_t last_index =
        static_cast<std::size_t>(last - data_);

    std::size_t count = last_index - first_index;

    // Dịch phần đuôi sang trái.
    for (std::size_t i = first_index; i + count < size_; ++i) {
        data_[i] = std::move(data_[i + count]);
    }

    // Destroy các phần tử dư ở cuối.
    for (std::size_t i = 0; i < count; ++i) {
        --size_;
        (*(data_ + size_)).~T();
    }

    return data_ + first_index;
}

private:
    // You decide the internal representation.
	static T* allocate_raw(std::size_t capacity) {
        void* raw = ::operator new(sizeof(T) * capacity, std::align_val_t(alignof(T)));
        T* data = static_cast<T*>(raw);
        return data;
    }
    static void deallocate_raw(T* ptr) noexcept {
        ::operator delete(ptr, std::align_val_t(alignof(T)));
    }

    void destroy_elements() noexcept {
        while (size_ > 0) {
            --size_;
            data_[size_].~T();
        }
    }

	template <typename... Args>
T& grow_and_emplace_back(Args&&... args) {
    const std::size_t old_size = size_;
    const std::size_t new_capacity =
        std::max<std::size_t>(1, capacity_ * 2);

    T* new_data = allocate_raw(new_capacity);

    std::size_t relocated = 0;
    bool new_element_constructed = false;

    try {
        // Construct phần tử mới trước.
        // args vẫn đang trỏ vào storage cũ hợp lệ.
        ::new (static_cast<void*>(new_data + old_size))
            T(std::forward<Args>(args)...);

        new_element_constructed = true;

        if constexpr (
            std::is_move_constructible_v<T> ||
            std::is_copy_constructible_v<T>
        ) {
            for (; relocated < old_size; ++relocated) {
                ::new (static_cast<void*>(new_data + relocated))
                    T(std::move_if_noexcept(data_[relocated]));
            }
        } else {
            // Kiểu hoàn toàn không relocate được chỉ có thể growth
            // khi vector hiện đang rỗng.
            if (old_size != 0) {
                throw std::runtime_error("T is not relocatable");
            }
        }
    } catch (...) {
        while (relocated > 0) {
            --relocated;
            new_data[relocated].~T();
        }

        if (new_element_constructed) {
            new_data[old_size].~T();
        }

        deallocate_raw(new_data);
        throw;
    }

    for (std::size_t i = 0; i < old_size; ++i) {
        data_[i].~T();
    }

    deallocate_raw(data_);

    data_ = new_data;
    capacity_ = new_capacity;
    size_ = old_size + 1;

    return *std::launder(data_ + old_size);
}

    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

} // namespace mini
