# `mini::RingDeque<T>` exercise

Implement a deque using a dynamically growing circular buffer. This is the
useful cache-friendly ring-buffer variant, not the segmented block-map normally
used by `std::deque`.

The API supports insertion/removal at both ends and constant-time indexed
access. It deliberately does not promise the reference stability of
`std::deque`: growing the buffer relocates every live element.

Do not use a standard sequence container internally.

## Checkpoint 1 — both ends and wrap-around

Implement:

- construction, destruction, `empty`, `size`, and `capacity`;
- `front`, `back`, and both overloads of `operator[]`;
- `emplace_front/back` and `push_front/back`;
- `pop_front/back` and `clear`;
- raw allocation, placement construction, and explicit destruction.

`clear()` destroys every live element immediately but retains the allocated
buffer and its capacity for reuse.

At this checkpoint, the tests never insert when `size() == capacity()`. You may
leave growth for checkpoint 2. The constructor must provide at least the
requested capacity; a request of zero may produce zero capacity.

The important indexing rule is:

```cpp
physical(logical_index) = (head_ + logical_index) % capacity_
```

Never perform this modulo while `capacity_ == 0`. Moving the head one slot to
the left can be written without unsigned underflow as:

```cpp
head_ = (head_ + capacity_ - 1) % capacity_;
```

From the repository root (pick the stage you are working on):

```sh
cmake --preset dev -DMINI_RING_DEQUE_STAGE=N
cmake --build --preset dev --target ring_deque_tests
ctest --preset dev -R ring_deque_tests
```

## Checkpoint 2 — growth and relocation

Implement automatic growth and `reserve`.

When the buffer is full, allocate a larger raw buffer and move-construct the
live elements into positions `[0, size())` in logical order—not physical array
order. Destroy the old elements, release the old allocation, install the new
buffer, and set `head_ = 0`.

`reserve(n)` never shrinks. If `n <= capacity()`, it does nothing and must not
invalidate existing references. Otherwise it relocates the elements exactly as
growth does.

Growth must work from zero capacity and from a full wrapped layout. Compile by
changing the stage to `2`.

## Checkpoint 3 — copy, move, swap, and fuzzing

Implement copy/move construction and assignment plus `swap`.

- Copies own independent storage and preserve logical order.
- Move transfers the allocation in `O(1)` without relocating elements.
- A moved-from deque is valid, empty, and reusable.
- Self-move assignment is a no-op.
- `swap` is `O(1)` and does not relocate elements.

Compile by changing the stage to `3`.

## Representation invariants

- `0 <= size_ <= capacity_`.
- `capacity_ == 0` implies `data_ == nullptr`, `size_ == 0`, and `head_ == 0`.
- Logical element `i` is the live `T` at
  `data_[(head_ + i) % capacity_]`.
- Exactly `size_` slots contain live `T` objects; every other slot is raw
  uninitialized storage.
- Empty and full are distinguished by `size_`, not by comparing head/tail
  indices.
- After removing the final element or calling `clear`, normalize `head_` to
  zero.
- Every constructed `T` is destroyed exactly once and every raw allocation is
  released exactly once.

`front`, `back`, and `operator[]` are `O(1)`. Pushes are amortized `O(1)` and
pops are `O(1)`. `reserve`, growth, and copy are `O(n)`; move and swap are
`O(1)`.

Growth/reserve invalidates every pointer and reference into the deque. A push
that does not grow preserves existing element addresses. A pop invalidates only
the removed element.

## Deliberately out of scope

- exception-safety guarantees and throwing element types;
- iterators and const iterators;
- `at`, middle insertion/erasure, and `shrink_to_fit`;
- allocators and segmented `std::deque` storage;
- passing a reference to an existing element back into a push/emplace operation
  that triggers growth.

## Definition of done

All three checkpoints compile as C++20 with the warning flags above and pass
under ASan/UBSan. There is no exception-safety phase.
