# `mini::Function<R(Args...)>` exercise

Implement a copyable type-erased callable wrapper similar to
`std::function`. This exercise focuses on callable type erasure, template
partial specialization, perfect forwarding, value semantics, and eventually
small-buffer optimization.

The implementation is in `include/mini/function.hpp`. Stage 2 is enabled by
default; select a different checkpoint with `-DMINI_FUNCTION_STAGE=N`.

## Why the class is a partial specialization

The primary template receives one type:

```cpp
template <class Signature>
class Function;
```

The specialization decomposes a function signature into its return and
argument types:

```cpp
template <class R, class... Args>
class Function<R(Args...)>;
```

For `Function<long(int, double)>`, `R` is `long` and `Args...` is
`int, double`.

## Checkpoint 1 — heap-backed type erasure and invocation

Define a non-template erased interface and a templated concrete model:

- `CallableBase` provides virtual destruction and virtual invocation;
- `CallableModel<F>` owns one decayed callable `F`;
- `Function` stores only a `CallableBase*`, so its type and size do not depend
  on the lambda, function pointer, or functor assigned to it.

Implement default/null construction, construction from a callable,
destruction, `operator bool`, and `operator()`.

Required behavior:

- accept free-function pointers, functors, capturing lambdas, and mutable
  lambdas;
- store `std::decay_t<F>`, never a reference to the constructor argument;
- invoke through `std::invoke` and forward the declared `Args...` correctly;
- support both non-void and `void` return types;
- invoking an empty function throws `std::bad_function_call`.

Do not implement copying or small-buffer optimization yet. Heap-allocate one
model per non-empty `Function`.

## Checkpoint 2 — copy/move value semantics

Add a virtual `clone()` operation to the erased interface. Implement copy/move
construction, copy/move assignment, assignment from `nullptr`, assignment from
a callable, and `swap`.

- Copies contain independent copies of the callable state.
- Moves transfer the erased target in constant time and leave the source empty.
- Self-copy and self-move are valid.
- Assignment provides the strong exception guarantee via construct-then-swap.

Because `Function` is copyable, a stored callable must itself be copy
constructible. A move-only lambda belongs in a separate future
`MoveOnlyFunction`, not in this class.

## Checkpoint 3 — constraints and callable edge cases

Constrain the templated constructor/assignment so that they participate only
when the decayed target is copy constructible and invocable with `Args...`, and
its result is compatible with `R`. Ensure the template cannot capture another
`Function` and hide the real copy/move constructors.

Cover null function pointers, reference return types, move-only arguments,
`std::reference_wrapper`, overloaded functors, and exceptions thrown by the
target. Target exceptions propagate unchanged.

## Checkpoint 4 — small-buffer optimization

Add fixed inline storage so suitably small, aligned, nothrow-movable callables
avoid heap allocation. The erased operations now need to invoke, destroy,
move, and clone a target whose storage may be inline or on the heap.

This checkpoint is optional but high-value for interviews: be ready to explain
alignment, object lifetime, why the buffer size is an implementation choice,
and why the no-throw move condition simplifies exception safety.

## Core invariants

- Empty means there is no live erased target and `operator bool()` is false.
- Every stored callable is destroyed exactly once using its concrete type.
- Copies never share mutable callable state unless the callable itself chooses
  to share it.
- After a move, exactly one `Function` owns the original target.
- `operator() const` must still be able to invoke a stored mutable lambda, as
  `std::function` does.
- No constructor may accidentally store a reference to a temporary callable.

## Deliberately out of scope initially

- allocators;
- `target_type()` and `target<T>()`;
- `noexcept` function signatures and cv/ref-qualified signatures;
- exact `std::function` ABI or a production-quality SBO policy.

## Running a checkpoint

```sh
cmake --preset dev -DMINI_FUNCTION_STAGE=N
cmake --build --preset dev --target function_tests
ctest --preset dev -R function_tests
```
