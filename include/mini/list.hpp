#pragma once

#include <cstddef>
#include <iterator>
#include <utility>
#include <cassert>

namespace mini {

template <class T>
class List {
private:
    struct NodeBase {
        NodeBase* prev;
        NodeBase* next;
        NodeBase(NodeBase* prev_, NodeBase* next_) noexcept 
        : prev{prev_}, next{next_} 
        {

        }
        NodeBase(const NodeBase&) = delete;
        NodeBase& operator=(const NodeBase&) = delete;
        NodeBase(NodeBase&&) = delete;
        NodeBase& operator=(NodeBase&&) = delete;
        virtual ~NodeBase() = default;
    };

    struct Node : NodeBase {
        template <class... Args>
        explicit Node(Args&&... args)
            : NodeBase{nullptr, nullptr},
              value(std::forward<Args>(args)...) {}

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;
        ~Node() override = default;
        T value;
    };

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;

    class iterator {
        friend class List;

    private:
        NodeBase* current_ = nullptr;

        explicit iterator(NodeBase* node) noexcept 
        : current_{node} 
        {

        }

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator() noexcept = default;

        reference operator*() const noexcept {
            return static_cast<Node*>(current_)->value;
        }

        pointer operator->() const noexcept {
            return &(static_cast<Node*>(current_)->value);
        }

        iterator& operator++() noexcept {
            current_ = current_->next;
            return *this;
        }

        iterator operator++(int) noexcept {
            iterator copy = *this;
            current_ = current_->next;
            return copy;
        }

        iterator& operator--() noexcept {
            current_ = current_->prev;
            return *this;
        }

        iterator operator--(int) noexcept {
            iterator copy = *this;
            current_ = current_->prev;
            return copy;
        }

        bool operator==(const iterator& other) const noexcept {
            return (other.current_ == current_);
        }

    };

    List() noexcept 
    : _size{0}
    {
        sentinel->prev = sentinel;
        sentinel->next = sentinel;
    }

    ~List() {
        clear();
        delete sentinel;

    }

    [[nodiscard]] bool empty() const noexcept {
        return _size == 0;

    }
    [[nodiscard]] size_type size() const noexcept {
        return _size;
    }

    reference front() {
        assert(!empty());
        return cast(sentinel->next)->value;
    }

    const_reference front() const {
        assert(!empty());
        return cast(sentinel->next)->value;
    }

    reference back() {
        assert(!empty());
        return cast(sentinel->prev)->value;
    }

    const_reference back() const {
        assert(!empty());
        return cast(sentinel->prev)->value;
    }
    

    template <class... Args>
    reference emplace_front(Args&&... args) {
        Node* newNode = new Node(std::forward<Args>(args)...);
        insert_helper(begin(), newNode);
        return newNode->value;
    }

    template <class... Args>
    reference emplace_back(Args&&... args) {
        Node* newNode = new Node(std::forward<Args>(args)...);
        insert_helper(end(), newNode);
        return newNode->value;
    }

    void push_front(const T& value) {
        Node* newNode = new Node(value);
        insert_helper(begin(), newNode);
    }

    void push_front(T&& value) {
        Node* newNode = new Node(std::move(value));
        insert_helper(begin(), newNode);
    }

    void push_back(const T& value) {
        Node* newNode = new Node(value);
        insert_helper(end(), newNode);
    }

    void push_back(T&& value) {
        Node* newNode = new Node(std::move(value));
        insert_helper(end(), newNode);
    }

    void pop_front() {
        assert(!empty());
        erase(begin());
    }

    void pop_back() {
        assert(!empty());
        erase(--end());
    }

    void clear() noexcept {
        while(_size > 0) {
            pop_front();
        }
    }

    iterator begin() noexcept {
        return iterator{sentinel->next};
    }

    iterator end() noexcept {
        return iterator{sentinel};
    }

    template <class... Args>
    iterator emplace(iterator pos, Args&&... args) {
        Node* newNode = new Node(std::forward<Args>(args)...);
        insert_helper(pos, newNode);
        return iterator{newNode};
    }

    iterator insert(iterator pos, const T& value) {
        Node* newNode = new Node(value);
        insert_helper(pos, newNode);
        return iterator{newNode};
    }

    iterator insert(iterator pos, T&& value) {
        Node* newNode = new Node(std::move(value));
        insert_helper(pos, newNode);
        return iterator{newNode};
    }

    iterator erase(iterator pos) {
        assert(pos != end());
        NodeBase* prev = pos.current_->prev;
        NodeBase* next = pos.current_->next;
        NodeBase* cur = pos.current_;
        prev->next = next;
        next->prev = prev;
        delete cur;
        _size--;
        return iterator{next};
    }

    List(const List& other)
    : List{}
    {
        const NodeBase* current = other.sentinel->next;
        while (current != other.sentinel) {
            push_back(static_cast<const Node*>(current)->value);
            current = current->next;
        }
    }

    List& operator=(const List& other) {
        if (&other == this) {
            return *this;
        }
        clear();
        const NodeBase* current = other.sentinel->next;

        while (current != other.sentinel) {
            push_back(static_cast<const Node*>(current)->value);
            current = current->next;
        }
        return *this;
    }

    List(List&& other) noexcept
    : List{}
    {
        std::swap(sentinel, other.sentinel);    
        std::swap(_size, other._size);
    }

    List& operator=(List&& other) noexcept {
        if (&other == this) {
            return *this;
        } else {
            clear();
            std::swap(sentinel, other.sentinel);    
            std::swap(_size, other._size);
        }
        return *this;
    }

    void swap(List& other) noexcept {
        std::swap(sentinel, other.sentinel);    
        std::swap(_size, other._size);
    }

private:
    
    Node* cast(NodeBase* ptr) noexcept {
        return static_cast<Node*>(ptr);
    }

    const Node* cast(const NodeBase* ptr) const noexcept {
        return static_cast<const Node*>(ptr);
    }

    void insert_helper(iterator pos, Node* newNode) {
        NodeBase* cur = pos.current_;
        NodeBase* prev = cur->prev;
        prev->next = newNode;
        newNode->prev = prev;
        newNode->next = cur;
        cur->prev = newNode;
        _size++;
    }

    NodeBase* sentinel = new NodeBase{nullptr, nullptr};
    size_type _size = 0;
};

} 
