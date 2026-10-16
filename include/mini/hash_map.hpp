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
    HashMap() noexcept
    : bucket_count_{1}, size_{0}, buckets_{nullptr}
    {
        buckets_ = new Node*[bucket_count_]{nullptr};
    }

    ~HashMap() {
        clear();
        delete[] buckets_;
    }

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    // Returns false, and changes nothing, when the key is already present.
    bool insert(const K& key, const V& value) {
        if (contains(key)) {
            return false;
        }
        if (size_ + 1 > bucket_count_) {
            reserve(std::max(8uz, bucket_count_ * 2));
        }
        size_++;
        std::size_t hash = Hash{}(key);
        std::size_t index = hash % bucket_count_;
        auto head = buckets_[index];
        if (head == nullptr) {
            Node* node = new Node{key, value, hash, nullptr};
            buckets_[index] = node;
        } else {
            while(head -> next != nullptr) {
                if (head -> key == key) {
                    return false;
                }
                head = head -> next;
            }
            if (head -> key == key) {
                return false;
            }
            Node* node = new Node{key, value, hash, nullptr};
            head -> next = node;
        }
        return true;
    }

    // Inserts a value-initialized V() first when the key is missing.
    V& operator[](const K& key) {
        if (!contains(key)) {
            insert(key, V{});
        }
        std::size_t hash = Hash{}(key);
        std::size_t index = hash % bucket_count_;
        auto head = buckets_[index];
        while(!(head -> key == key)) {
            head = head -> next;
        }      
        return head -> value;
    }

    // nullptr when the key is missing.
    V* find(const K& key) {
        std::size_t hash = Hash{}(key);
        std::size_t index = hash % bucket_count_;
        auto head = buckets_[index];
        while(head != nullptr && !(head -> key == key)) {
            head = head -> next;
        }      
        if (head == nullptr) return nullptr;
        return &(head -> value);
    }

    const V* find(const K& key) const {
        std::size_t hash = Hash{}(key);
        std::size_t index = hash % bucket_count_;
        auto head = buckets_[index];
        while(head != nullptr && !(head -> key == key)) {
            head = head -> next;
        }      
        if (head == nullptr) return nullptr;
        return &(head -> value);
    }

    bool contains(const K& key) const {
        if (find(key) == nullptr) return false;
        return true;
    }

    // Returns false when the key is missing.
    bool erase(const K& key) {
        if (!contains(key)) return false;
        --size_;
        std::size_t hash = Hash{}(key);
        std::size_t index = hash % bucket_count_;
        auto head = buckets_[index];
        if (head -> key == key) {
            buckets_[index] = head -> next;
            delete head;
        } else {
            while(!(head -> next -> key == key)) {
                head = head -> next;
            }
            Node* del = head -> next;
            head -> next = head -> next -> next;
            delete del;
        }
        return true;
    }

    size_type size() const noexcept {
        return size_;
    }

    bool empty() const noexcept {
        return size_ == 0;
    }

    void clear() noexcept {
        for (int i = 0; i < bucket_count_; ++i) {
            Node* head = buckets_[i];
            buckets_[i] = nullptr;
            while (head != nullptr) {
                Node* next = head -> next;
                delete head;
                head = next;
            }    
        }
        size_ = 0;
    }

    size_type bucket_count() const noexcept {
        return bucket_count_;
    }

    // size() / bucket_count(), or 0 when bucket_count() == 0.
    float load_factor() const noexcept {
        return static_cast<float>(size_)/static_cast<float>(bucket_count_);
    }

    // Ensures bucket_count() >= count. Never shrinks.
    void reserve(size_type count) {
        if (bucket_count() >= count) return;
        Node** new_buckets = new Node*[count]{nullptr};
        for (int i = 0; i < bucket_count_; i++) {
            Node* head = buckets_[i];
            while (head != nullptr) {
                Node* next = head -> next;
                std::size_t index = (head -> hash) % count;
                if (new_buckets[index] == nullptr) new_buckets[index] = head;
                else {
                    Node* new_head = new_buckets[index];
                    while(new_head -> next != nullptr) new_head = new_head -> next;
                    new_head -> next = head;
                }
                head -> next = nullptr;
                head = next;
            }
            buckets_[i] = nullptr;
        }
        delete[] buckets_;
        buckets_ = new_buckets;
        bucket_count_ = count;
    }

    // Checkpoint 2 adds no API: a pointer or reference to a value must stay
    // valid until that entry is erased, across every rehash.

private:
    // Representation: your choice. Write its invariants here.
    struct Node {
        K key;
        V value;
        std::size_t hash;   
        Node* next;
    };
    Node** buckets_ = nullptr;      
    std::size_t bucket_count_ = 0;
    std::size_t size_ = 0;
};

}  // namespace mini
