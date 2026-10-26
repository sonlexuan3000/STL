# `mini::SharedPtr<T>` exercise

Implement shared ownership using a type-erased control block. The stored view
pointer may differ from the pointer or object owned by the control block, so
ownership and pointer access must remain separate concepts.

## Completed checkpoints

### Checkpoint 1 — basic shared ownership

Implement default, raw-pointer, copy, and move construction; assignment;
destruction; `get`, dereference, `operator bool`, `use_count`, `reset`, and
`swap`. Preserve the original raw-pointer type in the control block so that
`SharedPtr<Base>(new Derived)` destroys `Derived` correctly even when `Base`
does not have a virtual destructor.

### Checkpoint 2 — converting ownership

Add converting copy and move operations constrained by pointer convertibility.
Pointer adjustment must work for multiple inheritance, and a move must transfer
the control block without changing its strong count.

### Checkpoint 3 — atomic reference counts

Make the strong and weak counts atomic. Copying distinct handles that share a
control block is safe across threads. Releasing the final strong reference uses
release/acquire synchronization before destroying the managed object.

### Checkpoint 4 — single-allocation `make_shared`

Store `T` directly inside an in-place control block, construct it with perfect
forwarding, and destroy it with `std::destroy_at`. The object dies when the
strong count reaches zero; storage remains until the final weak reference is
released.

### Checkpoints 5–6 — aliasing ownership

Implement copy and move aliasing constructors. The view pointer may point to a
member, a base subobject, `nullptr`, or unrelated external storage while the
control block continues to own the original object.

### Checkpoints 7–8 — pointer casts

Implement lvalue `static_pointer_cast` and `dynamic_pointer_cast` using the
aliasing constructor. A failed dynamic cast returns an empty owner and must not
increment the source count.

### Checkpoint 9 — custom deleters and constructor exceptions

Store the concrete deleter in `PointerControlBlock<U, Deleter>` and call it
through the virtual `destroy_object` interface. The one-argument raw-pointer
constructor delegates to `std::default_delete<U>`. If allocation or deleter
storage throws, invoke the supplied deleter exactly once and rethrow.

The current checkpoint deliberately supports copyable deleters. Tests cover a
stateful deleter, `malloc`/`free`, typed-null ownership, allocation failure, and
a throwing deleter copy.

## Ownership invariants

- An empty default-constructed handle has no control block and `use_count()==0`.
- A handle constructed from a null raw pointer owns a control block, has
  `use_count()==1`, but converts to `false`.
- `pointer_` is the exposed view; the control block stores the owned object or
  original pointer and its destruction policy.
- Exactly one operation changes a strong count from one to zero and destroys
  the managed object.
- The implicit weak reference exists while the strong count is nonzero. The
  control block is deleted only after both strong ownership and all explicit
  weak observers are gone.
- A custom deleter must not throw when invoked. Destruction is `noexcept`, so a
  throwing deleter terminates the program.
- Atomic counts protect shared ownership metadata only. They do not make the
  same smart-pointer handle, or the managed `T`, safe for concurrent mutation.

## High-value backlog

These are worth returning to for C++ interview preparation, but are not needed
before starting another component.

### Milestone A — `enable_shared_from_this` (highest priority)

- Implement `EnableSharedFromThis<T>` with an internal `WeakPtr<T>`.
- Initialize that observer when the first compatible `SharedPtr` takes
  ownership, including through `make_shared`.
- Implement `shared_from_this()` and `weak_from_this()` without creating a
  second control block.
- Decide and test the expired/unowned behavior (`std::bad_weak_ptr` for the
  throwing API).

This milestone tests friendship between templates, inheritance detection,
control-block reuse, and one of the most common real-world smart-pointer bugs.

### Milestone B — finish custom-deleter ergonomics

- Support a move-only deleter while preserving cleanup if storing it throws.
- Add `get_deleter<D>` using type information stored behind the control block.
- Add `reset(U*)` and `reset(U*, D)` via construction plus `swap`.

### Milestone C — remaining useful standard behavior

- Construct `SharedPtr` from a live `WeakPtr`, throwing `bad_weak_ptr` when it
  is expired.
- Add rvalue overloads of `static_pointer_cast` and `dynamic_pointer_cast`.
- Add non-member `swap` and equality with pointers/null.
- Polish in-place lifetime access with the pointer returned by
  `std::construct_at` or `std::launder` where appropriate.

## Low-return features to skip for now

- allocator-aware control blocks and `allocate_shared`;
- array specializations and overwrite construction;
- `owner_before`, owner hashing, and the complete standard overload set;
- matching a production standard library's ABI, compressed-pair layout, or
  platform-specific lock-free implementation.

## Running the checkpoints

From the repository root:

```sh
cmake --preset dev -DMINI_SHARED_PTR_STAGE=N
cmake --build --preset dev --target shared_ptr_tests shared_ptr_exception_tests
ctest --preset dev -R shared_ptr
```

Checkpoint 9 is the current stopping point. Resume with Milestone A when
returning to the smart-pointer project.
