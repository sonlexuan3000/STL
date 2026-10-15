# `mini::HashMap<K, V, Hash>` exercise

Implement a node-based hash table with the core behaviour of
`std::unordered_map`: unique keys, expected `O(1)` insert/lookup/erase, a load
factor that never exceeds 1, and references that survive rehashing.

This is the counterpart to [`FlatHashMap`](flat_hash_map.md). The flat map
stores entries inline for cache locality and therefore moves them on every
rehash; this map trades locality for reference stability, which is the
guarantee `std::unordered_map` gives.

Do not use any standard container (`std::vector`, `std::list`,
`std::unordered_map`, …) inside the implementation, and keep the header valid
C++17. The tests may use standard containers as a reference model.

Inspired by the [WhiteBox "Hash Map"](https://whitebox.ac/problems/hash-map)
problem.

## Checkpoint 1 — insert, lookup, erase

Implement every member declared in `include/mini/hash_map.hpp`.

From the repository root:

```sh
cmake --preset dev -DMINI_HASH_MAP_STAGE=1
cmake --build --preset dev --target hash_map_tests
ctest --preset dev -R hash_map_tests
```

Semantics:

- Keys are hashed with a default-constructed `Hash` and compared **only** with
  `operator==`. The tests use a key type that has no other comparison operator.
- `insert` never overwrites. Inserting a present key returns `false` and
  constructs nothing.
- `operator[]` value-initializes a `V()` only when the key is missing and
  returns a reference to the stored value either way.
- A default-constructed map may have zero buckets; `load_factor()` is then 0.
- After every insertion, `size() <= bucket_count()`. Growth is geometric.
- `reserve(n)` makes `bucket_count() >= n`, so inserting up to `n` entries in
  total does not change `bucket_count()`. It never shrinks the table.
- Each key and value is constructed once when its entry is added and destroyed
  exactly once when it is erased, cleared, or the map is destroyed. Lookups and
  failed inserts construct no `K` or `V`.

Expected cost with a good hash is `O(1)` per operation. The tests count `==`
calls over a long random workload and fail if the average per operation grows
beyond a small constant. A hash that maps every key to the same bucket must
still give correct results (just slowly).

## Checkpoint 2 — reference stability

No new API. A pointer or reference to an entry's value must stay valid, and
keep pointing to that entry, until that entry itself is erased — through any
number of rehashes, `reserve` calls, and erasures of other entries.

Compile with `-DMINI_HASH_MAP_STAGE=2`.

## Things to think about

Write your representation's invariants as a comment in the header. Questions
worth answering there:

- What does a bucket hold, and how is an empty bucket represented?
- How does a rehash move entries between buckets without moving the `K`/`V`
  objects themselves?
- How do you avoid calling `operator==` on keys whose hashes already differ?
- How is the bucket index derived from the hash, and what happens with a hash
  whose low bits are all equal?
- Which operations may allocate, and what is left behind if an allocation or a
  `K`/`V` copy throws?

## Deliberately out of scope

- iterators, `rehash`, and a configurable maximum load factor;
- copy/move construction and assignment (copy is deleted);
- custom equality predicates and allocators;
- heterogeneous lookup and `emplace`.

## Submitting to WhiteBox

The judge compiles C++17, provides the standard headers, owns `main`, and
expects a global `HashMap` class. Paste the class template (everything inside
`namespace mini`) without the namespace. The judge also macro-defines standard
container names away, so they may not appear anywhere in the class, not even as
member or variable names; the `hash_map_whitebox_check` build target emulates
that rule locally.

## Definition of done

Both checkpoints compile as C++17 with the project's warning flags and pass
under ASan/UBSan.
