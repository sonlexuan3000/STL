#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace mini {

// Training exercise: an open-addressed hash map with linear probing.
//
// Define every member in this header. The checkpoint contract and build
// commands are in README.md. Do not use std::map or std::unordered_map inside
// the implementation.
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class FlatHashMap {
public:
    using key_type = Key;
    using mapped_type = T;
    using size_type = std::size_t;

    // Checkpoint 1: construction, insertion, and lookup.
    explicit FlatHashMap(size_type initial_capacity = 8,
                         Hash hash = Hash{},
                         KeyEqual equal = KeyEqual{});

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_type size() const noexcept;
    [[nodiscard]] size_type capacity() const noexcept;

    bool insert(Key key, T value);

    T* find(const Key& key);
    const T* find(const Key& key) const;
    [[nodiscard]] bool contains(const Key& key) const;

    // Checkpoint 2: tombstones and destruction.
    bool erase(const Key& key);
    void clear() noexcept;

    // Checkpoint 3: growth and value semantics.
    [[nodiscard]] double load_factor() const noexcept;
    void reserve(size_type element_count);
    void rehash(size_type bucket_count);

    FlatHashMap(const FlatHashMap& other);
    FlatHashMap& operator=(const FlatHashMap& other);
    FlatHashMap(FlatHashMap&& other);
    FlatHashMap& operator=(FlatHashMap&& other);
    ~FlatHashMap() = default;

    void swap(FlatHashMap& other);

private:
    enum class State : unsigned char {
        empty,
        occupied,
        deleted,
    };

    struct Entry {
        Key key;
        T value;
    };

    struct Bucket {
        State state = State::empty;
        std::optional<Entry> entry;
    };

    struct ProbeResult {
        size_type index;
        bool found;
    };

    static constexpr size_type npos = static_cast<size_type>(-1);
    static constexpr size_type default_capacity = 8;

    // A lookup may stop at State::empty, but it must continue through
    // State::deleted. Insertion remembers the first deleted bucket while still
    // probing for an already-existing equivalent key.
    [[nodiscard]] size_type find_index(const Key& key) const;
    [[nodiscard]] ProbeResult probe_for_insert(const Key& key) const;

    void ensure_room_for_insert();
    void rebuild(size_type requested_capacity);

    std::unique_ptr<Bucket[]> buckets_;
    size_type size_ = 0;
    size_type capacity_ = 0;
    size_type deleted_ = 0;
    [[no_unique_address]] Hash hash_;
    [[no_unique_address]] KeyEqual equal_;
};

} // namespace mini
