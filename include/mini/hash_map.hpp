#pragma once

#include <cstddef>
#include <functional>
#include <new>
#include <utility>

namespace mini {

// Training exercise: a node-based hash map with the core semantics of
// std::unordered_map (unique keys, max load factor 1, stable references).
//
// Define every member in this header. The checkpoint contract and build
// commands are in docs/hash_map.md.
//
// Keep this header C++17 and self-contained (standard headers only), and do
// not use any standard container, so the class can be submitted to an online
// judge as-is. The `hash_map_whitebox_check` target enforces both.
template <class K, class V, class Hash = std::hash<K>>
class HashMap {
public:
    using key_type = K;
    using mapped_type = V;
    using hasher = Hash;
    using size_type = std::size_t;

    // Checkpoint 1: insert, lookup, erase, and growth.
    HashMap() noexcept;
    ~HashMap();

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    // Returns false, and changes nothing, when the key is already present.
    bool insert(const K& key, const V& value);

    // Inserts a value-initialized V() first when the key is missing.
    V& operator[](const K& key);

    // nullptr when the key is missing.
    V* find(const K& key);
    const V* find(const K& key) const;

    bool contains(const K& key) const;

    // Returns false when the key is missing.
    bool erase(const K& key);

    size_type size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;

    size_type bucket_count() const noexcept;

    // size() / bucket_count(), or 0 when bucket_count() == 0.
    float load_factor() const noexcept;

    // Ensures bucket_count() >= count. Never shrinks.
    void reserve(size_type count);

    // Checkpoint 2 adds no API: a pointer or reference to a value must stay
    // valid until that entry is erased, across every rehash.

private:
    // Representation: your choice. Write its invariants here.
};

}  // namespace mini
