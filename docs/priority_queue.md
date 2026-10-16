# `mini::PriorityQueue<T, Container, Compare>` exercise

Implement a priority-queue container adaptor backed by a binary heap. The
default underlying container is `mini::Vector<T>`, while `Compare` controls
which value has the highest priority.

If `compare(a, b)` is `true`, `a` has lower priority than `b`. Consequently,
`std::less<T>` creates a max-heap and `std::greater<T>` creates a min-heap.

## Checkpoint 1 — core heap operations

Implement construction, `empty`, `size`, `top`, `push`, `emplace`, and `pop`.
Insertion appends an element and restores the heap with `sift_up`; removal
moves the last element to the root and restores the heap with `sift_down`.

## Checkpoint 2 — generic construction

Implement construction from copied and moved containers. An arbitrary input
container is not already a heap, so restore the invariant with bottom-up
heapification. Starting from the last internal node and applying `sift_down`
must take `O(n)` comparisons overall.

The implementation must support stateful comparators and move-only values.
`swap` exchanges both the container and comparator so they remain consistent.

## Checkpoint 3 — value semantics and fuzzing

Rely on the container and comparator for copy/move/destruction through the Rule
of Zero. Copies are independent, moves remain valid, and randomized operation
sequences must match `std::priority_queue` for max-heaps and min-heaps.

From the repository root:

```sh
cmake --preset dev -DMINI_PRIORITY_QUEUE_STAGE=N
cmake --build --preset dev --target priority_queue_tests
ctest --preset dev -R priority_queue_tests
```

## Invariants and complexity

- For every child, its parent has at least as much priority.
- The root at index zero is the value returned by `top()`.
- Parent index: `(index - 1) / 2`.
- Child indices: `2 * index + 1` and `2 * index + 2`.
- `top`, `empty`, and `size` are `O(1)`.
- `push`, `emplace`, and `pop` are `O(log n)`.
- Construction from an existing container is `O(n)`.

Calling `top` or `pop` on an empty queue violates the precondition, matching
`std::priority_queue`.

## Deliberately out of scope

- iterators and direct access to the underlying container;
- allocator-aware and iterator-range constructors;
- C++23 `push_range`;
- exception-safety guarantees for throwing values or comparators.

## Definition of done

All three checkpoints compile as C++20 with project warning flags and pass
under ASan/UBSan, including the deterministic differential tests.
