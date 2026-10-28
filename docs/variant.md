# `mini::Variant<Ts...>`

A type-safe tagged union: one object that holds a value of exactly one of
`Ts...` at a time, stored in place, plus a runtime index saying which one.

The interesting problem is the bridge between two worlds: the **index is only
known at run time**, but every operation on the value (destroy it, copy it,
pass it to a visitor) needs its **type at compile time**.

Implement the declarations in `include/mini/variant.hpp`. Do not use
`std::variant`, `std::visit`, `std::aligned_storage`, `std::any`, `std::max`,
`sizeof...`, or `<type_traits>` in the implementation; use `mini/type_list.hpp`
and `mini/type_traits.hpp`. The tests may use the standard library as an
oracle.

## Stage 1 — Storage, construction, destruction, access

Implement:

- in-place storage big enough and aligned enough for every alternative, and
  the active index;
- `Variant()` (value-initialized first alternative) and the converting
  constructor from an alternative;
- the destructor;
- `index()`, `variant_size`, `variant_alternative`, `holds_alternative`,
  `get<I>`, `get<T>`, and `get_if<T>`.

Rules:

- `sizeof(Variant<Ts...>)` must not grow with the *sum* of the alternatives'
  sizes: the storage is shared, like a union. Alignment must be at least the
  largest alternative alignment.
- The converting constructor accepts `T&&` and is enabled only when
  `remove_cvref_t<T>` appears exactly once in `Ts...`. It must not hijack copy
  construction of the variant itself (stage 2).
- `get` with the wrong alternative throws `mini::bad_variant_access`;
  `get_if` returns `nullptr` instead (also when passed a null pointer). `get<T>`
  and `holds_alternative<T>` require `T` to appear exactly once
  (`static_assert`); `get<I>` works with repeated types.
- Exactly one alternative object is alive at any time, and it is destroyed
  exactly once.

The destructor is the first place the two worlds meet: it knows `index()` at
run time and must call the destructor of the right type. A compile-time
recursion over the alternatives that checks the runtime index at each level
(`if constexpr` to stop, ordinary `if` to compare) is enough here.

Run:

```sh
cmake --preset dev -DMINI_VARIANT_STAGE=1
cmake --build --preset dev --target variant_tests
ctest --preset dev -R '^variant_tests$'
```

## Stage 2 — Value semantics, `emplace`, assignment

Implement copy/move construction, copy/move assignment, `emplace<I>(args...)`,
`emplace<T>(args...)`, and assignment from an alternative.

- Copy/move construction builds the same alternative from the other variant's
  value.
- Assignment when both hold the **same** alternative assigns the value
  (`T::operator=`), without destroying it. When they hold **different**
  alternatives, destroy the current value, then construct the new one.
- Self-assignment is a no-op. A moved-from variant keeps its index; its value
  is whatever the alternative's move left behind.
- `emplace` destroys the current value, constructs the new alternative in
  place from `args...`, and returns a reference to it.
- Assigning an alternative `T` assigns if the variant already holds `T`,
  otherwise destroys and constructs.
- Moving a variant is `noexcept` when every alternative's move is.

You may assume the alternatives' copy/move constructors do not throw;
exception safety (`valueless_by_exception`) is out of scope.

Run the commands above with `MINI_VARIANT_STAGE=2`.

## Stage 3 — `visit` and `operator==`

- `visit(f, v)` calls `f` with the active alternative (a reference, const for a
  const variant) and returns what `f` returns. `f` must return the same type
  for every alternative; reject other visitors with a readable
  `static_assert`.
- Dispatch must not be a chain of comparisons that grows with the number of
  alternatives: build a compile-time table with one function pointer per
  alternative and index it with `index()`. This is how `std::visit` gets O(1)
  dispatch.
- `operator==` is true when both variants hold the same index and the values
  compare equal.

Run the commands above with `MINI_VARIANT_STAGE=3`.

## Backlog (optional)

- `visit` over several variants at once (a multi-dimensional table).
- The full `std::variant` converting-constructor rules (overload resolution
  among alternatives instead of an exact type match).
- `valueless_by_exception` and the strong guarantee for assignment.
- `constexpr` construction and access.
