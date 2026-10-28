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

## Stage 4 — Detecting valid expressions

Answer "can I write this expression with a T?" in two ways, and compare them.

C++17 detection idiom (class templates):

- `void_t<Ts...>`, an alias for `void`;
- `has_size<T>` / `has_size_v<T>`: true when `std::declval<T&>().size()` is a
  valid expression;
- `is_equality_comparable<T>` / `is_equality_comparable_v<T>`: true when
  `std::declval<const T&>() == std::declval<const T&>()` is valid.

The primary template takes a second parameter defaulted to `void` and derives
from `false_type`; a partial specialization whose second argument is
`void_t<decltype(expression)>` derives from `true_type`.

C++20 concepts (requires-expressions):

- `Sized<T>`: for a `const T& t`, `t.size()` is convertible to `std::size_t`;
- `EqualityComparable<T>`: for `const T& a, b`, `a == b` is convertible to
  `bool`.

The two families deliberately ask slightly different questions, and the tests
check the difference:

- `has_size` uses a non-const lvalue, `Sized` a const one, so a type whose
  `size()` is not `const` has `has_size_v == true` but is not `Sized`;
- the traits only check that the expression is valid, the concepts also check
  its result type, so `size()` returning `std::string`, or `==` returning
  `void`, passes the trait but not the concept.

The tests also overload a function on `Sized` and on a stricter concept built
as `Sized<T> && ...`, so your concept must be usable for subsumption.

For this stage only, the implementation may include `<utility>`
(`std::declval`) and `<concepts>` (`std::convertible_to`).

Run the same commands with `MINI_TYPE_TRAITS_STAGE=4`.

## Rules

- Do not include or delegate to `<type_traits>` in the implementation (stage 4
  may use `<utility>` and `<concepts>`).
- Prefer a primary template plus the smallest necessary specializations.
- Keep all observable results usable in constant expressions.
- Stop at the current stage; later declarations may remain incomplete.
