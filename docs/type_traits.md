# Mini type traits

This exercise builds the compile-time primitives behind `<type_traits>`.
Implement the declarations in `include/mini/type_traits.hpp` without using
their `std::` equivalents. The tests may use `<type_traits>` as an oracle.

## Stage 1 — Compile-time values and predicates

Implement:

- `integral_constant<T, Value>` with `value_type`, `type`, static `value`, a
  `constexpr noexcept` conversion operator, and `constexpr noexcept`
  `operator()`;
- `bool_constant`, `true_type`, and `false_type`;
- `is_same<T, U>` and `is_same_v<T, U>`.

The central idea is that class-template specialization performs pattern
matching on types. `is_same<T, U>` is false by default, while the pattern
`is_same<T, T>` is true.

Run:

```sh
cmake --preset dev -DMINI_TYPE_TRAITS_STAGE=1
cmake --build --preset dev --target type_traits_tests
ctest --test-dir build/dev -R '^type_traits_tests$' --output-on-failure
```

## Stage 2 — Type transformations

Implement:

- `remove_const`, `remove_volatile`, and `remove_cv`;
- `remove_reference` for both lvalue and rvalue references;
- `remove_cvref` by composing earlier transformations;
- every matching `_t` alias.

Only top-level cv-qualifiers are removed. For example, removing const from
`const int*` must preserve the const qualification of the pointed-to `int`.

Run the same commands with `MINI_TYPE_TRAITS_STAGE=2`.

## Stage 3 — Selection and SFINAE

Implement:

- `conditional` and `conditional_t`;
- `enable_if` and `enable_if_t`.

`enable_if<false, T>` intentionally has no nested `type`. When that missing
type appears in an immediate substitution context, the candidate is removed
instead of causing the whole compilation to fail: substitution failure is not
an error (SFINAE).

Run the same commands with `MINI_TYPE_TRAITS_STAGE=3`.

## Rules

- Do not include or delegate to `<type_traits>` in the implementation.
- Prefer a primary template plus the smallest necessary specializations.
- Keep all observable results usable in constant expressions.
- Stop at the current stage; later declarations may remain incomplete.
