# `mini::FlatHashMap<Key, T>` exercise

Implement an open-addressed hash map using linear probing. This exercise is
about collision handling, tombstones, load factor, rehashing, and value
semantics. It deliberately does not try to clone every overload of
`std::unordered_map`.

The provided representation uses `std::optional` and `std::unique_ptr` so this
exercise stays focused on hashing rather than repeating raw object-lifetime
work from `Vector` and `List`.

Do not use `std::map`, `std::unordered_map`, `std::set`, or
`std::unordered_set` inside the implementation. They are allowed in tests as a
reference model.

## Checkpoint 1 — insertion and lookup

Implement:

- construction with at least the requested number of buckets (and at least one
  bucket even when the request is zero);
- `empty`, `size`, and `capacity`;
- `insert(Key, T)`, `find`, and `contains`;
- collision resolution by linear probing, including wrap-around.

`insert` returns `true` only when it inserts a new key. An equivalent existing
key is not overwritten, and `size()` does not change.

From the repository root (pick the stage you are working on):

```sh
cmake --preset dev -DMINI_FLAT_HASH_MAP_STAGE=N
cmake --build --preset dev --target flat_hash_map_tests
ctest --preset dev -R flat_hash_map_tests
```

## Checkpoint 2 — erase and tombstones

Implement `erase` and `clear`.

An erased bucket becomes `deleted`, not `empty`. Lookup must continue through a
deleted bucket because another colliding key may appear later in the probe
cluster. Insertion may reuse the first tombstone it sees, but it must continue
probing far enough to prove that the key is not already present.

`erase(key)` returns whether an element was erased and destroys that entry
immediately. `clear()` keeps the allocated bucket array and leaves every bucket
in the empty state.

Compile by changing the stage to `2`.

## Checkpoint 3 — growth and value semantics

Implement:

- automatic growth before the table runs out of usable buckets;
- `load_factor`, `reserve`, and `rehash`;
- copy/move construction and assignment;
- `swap`.

Choose a maximum load factor around `0.7`. Account for tombstones when deciding
whether probing is becoming too crowded. Rehashing copies or moves every live
entry into a fresh table and removes all tombstones.

`reserve(n)` must leave enough buckets for at least `n` elements without an
immediate rehash. `rehash(n)` chooses at least `n` buckets and must also choose
enough space for all current elements.

Copies must be independent. For this exercise a moved-from map must be empty,
valid, and reusable. Copy is `O(n)`; move and swap are `O(1)`.

Compile by changing the stage to `3`.

## Probe rules and invariants

- `size_` equals the number of occupied buckets.
- `deleted_` equals the number of tombstone buckets.
- An occupied bucket owns exactly one `Entry`; empty/deleted buckets own none.
- Only an empty bucket ends an unsuccessful lookup. A deleted bucket does not.
- A probe examines at most `capacity()` buckets and must never loop forever.
- Equivalent keys are determined by `KeyEqual`, not `operator==`.
- The initial bucket is computed with `Hash`; do not assume that keys are
  integers.

Expected average complexity for `insert`, `find`, and `erase` is `O(1)`;
worst-case complexity is `O(n)`. Rehashing and copying are `O(n)`.

Rehashing invalidates every pointer returned by `find`. Insertion without a
rehash does not invalidate pointers to existing entries. Erasing invalidates
only the pointer to the erased entry.

## Deliberately out of scope

- iterators and const iterators;
- `operator[]`, `at`, node handles, and heterogeneous lookup;
- allocator support;
- exception-safety guarantees;
- matching the reference stability rules of node-based
  `std::unordered_map`.

## Definition of done

All three checkpoints compile as C++20 with the warning flags above and pass
under ASan/UBSan. There is no exception-safety bonus for this exercise.
