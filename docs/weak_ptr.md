# `mini::WeakPtr<T>` exercise

Implement non-owning observation of an object managed by `mini::SharedPtr`.
`WeakPtr` keeps the control block alive, but it must not keep the managed
object alive. Access to the object is only possible by atomically creating a
temporary `SharedPtr` through `lock()`.

## Checkpoint 1 — split object lifetime from control-block lifetime

Add an atomic weak counter to the control block. Use the implicit-weak-reference
invariant:

- `weak_count` starts at one while at least one strong owner exists;
- every `WeakPtr` adds one explicit weak reference;
- the last strong owner destroys the object, then releases the implicit weak
  reference;
- the thread that reduces `weak_count` from one to zero deletes the control
  block.

Implement default, copy, move, construction from `SharedPtr<T>`, assignment,
destruction, `reset`, `swap`, `use_count`, and `expired`.

`use_count()` reads the strong count. `expired()` is true exactly when the
strong count is zero. A typed-null `SharedPtr` still represents live ownership,
so its `WeakPtr` is not expired until that owner is released.

## Checkpoint 2 — atomic `lock()`

Implement `lock()` so it returns a `SharedPtr` sharing the existing control
block when the strong count is nonzero, or an empty `SharedPtr` after the object
has expired.

The check and increment must be one compare-exchange loop. A separate load
followed by `fetch_add` is incorrect: the final owner could destroy the object
between those operations. Never call the raw-pointer `SharedPtr` constructor
from `lock()`, because that would create a second control block for the same
pointer.

## Checkpoint 3 — conversions and contention

Add converting copy/move constructors constrained by
`std::is_convertible_v<U*, T*>`, both from `SharedPtr<U>` and `WeakPtr<U>`.
Preserve the adjusted view pointer as well as the shared control block. The
tests cover multiple inheritance and race `lock()` against the final strong
release from several threads.

## Checkpoint 4 — observe converting `SharedPtr` rvalues without consuming them

Construct directly from `const SharedPtr<U>&`, constrained by
`std::is_convertible_v<U*, T*>`. Passing `std::move(shared)` must only bind that
reference; it must not create a converting `SharedPtr<T>` temporary or move the
strong ownership out of `shared`. Creating a `WeakPtr` from any `SharedPtr`
observes its control block and never consumes a strong reference.

From the repository root:

```sh
cmake --preset dev -DMINI_WEAK_PTR_STAGE=N
cmake --build --preset dev --target weak_ptr_tests
ctest --preset dev -R weak_ptr_tests
```

## Thread-safety boundary

Distinct `SharedPtr` and `WeakPtr` handles may update the same control block
concurrently. Concurrently mutating the same handle is not safe, and neither
smart pointer makes the managed `T` itself thread-safe.

`expired()` and `use_count()` are snapshots. Do not check `expired()` and then
access the object; another thread may release the final owner between those
steps. Call `lock()` and test the returned `SharedPtr` instead.

## Deliberately out of scope

- throwing construction of `SharedPtr` directly from an expired `WeakPtr`;
- custom deleters and allocators;
- `enable_shared_from_this`;
- exception-safety guarantees.
