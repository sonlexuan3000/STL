# `mini::List<T>` exercise

Implement an STL-like doubly linked list in `include/mini/list.hpp`. The goal is not to
clone every overload of `std::list`; it is to practise ownership, pointer
invariants, iterators, and value semantics. Exception safety is an optional
bonus checkpoint after the core container is complete.

Do not modify `tests/list_tests.cpp` to make a failure disappear. Fix the container.
Do not use `std::list` or another standard sequence container internally.

## Checkpoint 1 — ownership

Implement:

- default constructor and destructor;
- `empty`, `size`, `front`, and `back`;
- `emplace_front/back` and `push_front/back`;
- `pop_front/back` and `clear`.

From the repository root (pick the stage you are working on):

```sh
cmake --preset dev -DMINI_LIST_STAGE=N
cmake --build --preset dev --target list_tests
ctest --preset dev -R list_tests
```

Required complexity:

- `empty`, `size`, `front`, `back`: `O(1)`;
- insertion/removal at either end: `O(1)`;
- `clear` and destruction: `O(n)`.

## Checkpoint 2 — mutable bidirectional iterator

This reduced exercise supports mutable iteration only. Define `iterator`, then
implement `begin/end`. The iterator must support:

- `*`, `->`, pre/post `++`, and pre/post `--`;
- default construction, copying, assignment, and equality comparison;
- `value_type`, `difference_type`, `reference`, `pointer`, and
  `iterator_category` (optionally also `iterator_concept`);
- `--end()` for a non-empty list;
- use with standard algorithms requiring a bidirectional iterator.

Compile by changing the stage to `2`.

## Checkpoint 3 — mutation and value semantics

Implement:

- `emplace(pos, ...)`, both `insert` overloads, and `erase`;
- copy/move construction and assignment;
- `swap`.

Compile with `-DMINI_LIST_STAGE=3`.

Copy operations must produce independent nodes. For this exercise, a moved-from
list must be a valid empty list. Move construction must steal the chain in
`O(1)` and be `noexcept`. Move assignment first destroys the destination's old
nodes in `O(size())`, then steals the source chain in `O(1)`; it is also
`noexcept`. Moving must not relocate the source's element objects.

## Checkpoint 4 — exception safety (optional bonus)

Only do this after stages 1–3 pass. No new public API is needed. The tests check
that failed element construction does not modify the list and that failed copy
assignment preserves the destination.

Compile with `-DMINI_LIST_STAGE=4`. It is fine to postpone this checkpoint.

## Representation and invariants

A useful representation is a sentinel which contains links but no `T`:

```cpp
struct NodeBase {
    NodeBase* prev;
    NodeBase* next;
};

struct Node : NodeBase {
    T value;
};
```

You may choose another correct design. Whichever design you use, write these
invariants as comments next to the representation:

- empty means both sentinel links point back to the sentinel;
- for every live link, `x->next->prev == x` and `x->prev->next == x`;
- `size_` equals the number of value-containing nodes;
- the sentinel is never dereferenced as a `Node`;
- every node is deleted exactly once.

Iterator invalidation contract:

- inserting does not invalidate existing iterators;
- erasing invalidates only iterators to the erased node;
- `erase(pos)` returns the iterator following the erased node.

Undefined operations, matching the educational STL contract, include
dereferencing `end()`, popping an empty list, erasing `end()`, incrementing
`end()`, and decrementing `begin()`.

## Definition of done

Stages 1–3 must compile as C++20 with the warning flags above, pass under
ASan/UBSan, and leave no live tracked objects after each test scope. Stage 4 is
extra credit rather than a prerequisite for moving to the next container.
