# `mini::type_list` and `mini::Tuple<Ts...>`

This exercise applies variadic templates in two steps: first recursion over a
compile-time list of types, then a heterogeneous container whose layout is
generated from that list.

Implement the declarations in `include/mini/type_list.hpp` and
`include/mini/tuple.hpp`. Do not use `std::tuple`, `std::get`,
`sizeof...`, or `<type_traits>` in the implementation; the tests may use them
as an oracle. You may use the traits from `mini/type_traits.hpp`.

## Stage 1 — Recursion over type lists

Implement `length`, `front`, `push_front`, `contains`, and `at` (with their
`_v` / `_t` aliases).

Every recursive metafunction has the same shape:

- a primary template, usually only declared;
- a specialization for the empty list `type_list<>` (the base case);
- a specialization for `type_list<Head, Tail...>` that recurses on
  `type_list<Tail...>`.

`front` of an empty list must have no `type`. `at` only has to work for valid
indices. It recurses on two things at once: the index and the list.

Run:

```sh
cmake --preset dev -DMINI_TUPLE_STAGE=1
cmake --build --preset dev --target tuple_tests
ctest --preset dev -R '^tuple_tests$'
```

## Stage 2 — Tuple storage and `get<I>`

Implement `Tuple<Ts...>`, `tuple_size`, `tuple_element`, and both overloads of
`get<I>`.

- `Tuple()` value-initializes every element; `Tuple(const Ts&...)` copies each
  argument into its element.
- `get<I>` returns a reference to element `I` (const for a const tuple).
  Writing through it changes the tuple.
- Copying a tuple copies each element; moving moves each element. Move-only
  element types such as `std::unique_ptr` must work.
- `tuple_element_t<I, Tuple<Ts...>>` is the `I`-th type; reuse `at_t`.

Two classic representations, pick one and write its invariant in the header:

1. **Recursive inheritance.** `Tuple<Head, Tail...>` derives from
   `Tuple<Tail...>` and stores one `Head`. `get<I>` walks down `I` bases.
2. **Indexed leaves.** One base class per element, each tagged with its index
   (`Leaf<0, int>`, `Leaf<1, double>`, ...). `get<I>` converts the tuple to the
   one base with that index. This needs a way to produce `0, 1, ..., N-1`
   (look up `std::index_sequence`, or build your own with a type list).

Think about: in which order do the elements end up in memory, and what is
`sizeof(Tuple<char, int, char>)` compared with a hand-written struct?

Run the commands above with `MINI_TUPLE_STAGE=2`.

## Stage 3 — Lookup by type, `make_tuple`, `==`, structured bindings

Implement `index_of`, `get<T>`, `get<I>` for rvalue tuples, `make_tuple`,
`operator==`, and the `std::tuple_size` / `std::tuple_element`
specializations.

- `get<T>` finds the element by type; `T` must appear exactly once. Reject
  other cases with a `static_assert` that has a readable message.
- `make_tuple(args...)` strips references and cv-qualifiers from each argument
  type: `make_tuple(ci)` with `const int ci` gives `Tuple<int>`.
- `operator==` compares element by element; `Tuple<>` objects are equal.
- Structured bindings use the tuple-like protocol: they read
  `std::tuple_size<E>::value`, `std::tuple_element<I, E>::type`, and call
  `get<I>(e)` found by argument-dependent lookup. Specializing those two
  `std` templates for `mini::Tuple` is allowed.
- `auto [a, b] = tuple;` copies the tuple into a hidden variable `e` and calls
  `get<I>` on it as an **xvalue**. With only the `&` and `const&` overloads,
  that call picks `const&` and the bindings fail to compile, so `get<I>`
  needs a `Tuple&&` overload returning `tuple_element_t<I, ...>&&`.

Run the commands above with `MINI_TUPLE_STAGE=3`.

## Backlog (optional)

- A perfect-forwarding constructor so `Tuple<std::string>(std::string(...))`
  moves instead of copying.
- Empty-base optimization so `Tuple<Empty, int>` is no larger than `int`.
- `get<T>` overloads for rvalue tuples.
- `apply(f, tuple)` and `tuple_cat`.
