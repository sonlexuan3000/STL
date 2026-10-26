# mini-stl

[![CI](https://github.com/sonlexuan3000/STL/actions/workflows/ci.yml/badge.svg)](https://github.com/sonlexuan3000/STL/actions/workflows/ci.yml)

Header-only re-implementations of C++ standard library containers and
utilities, written from scratch in C++20 to study what the standard library
does under the hood: raw allocation, placement construction, object lifetime,
exception safety, iterator invalidation, and cache-friendly layouts.

Every container manages its own storage (no standard sequence containers
inside) and is tested under AddressSanitizer and UndefinedBehaviorSanitizer
with lifetime-counting element types, move-only and non-default-constructible
types, and over-aligned types. Later checkpoints add randomized operation
sequences checked against the `std` equivalent.

## Components

| Component | Header | Status | Notes |
|---|---|---|---|
| `mini::Vector<T>` | [`vector.hpp`](include/mini/vector.hpp) | Done | Strong exception guarantee for copy-assign, `reserve`, `push_back`, `emplace_back`; aliasing-safe growth |
| `mini::List<T>` | [`list.hpp`](include/mini/list.hpp) | Stages 1–3 | Sentinel-based doubly linked list, bidirectional iterator. Stage 4 (exception safety) in progress — [spec](docs/list.md) |
| `mini::RingDeque<T>` | [`ring_deque.hpp`](include/mini/ring_deque.hpp) | Done | Growable circular buffer with copy/move/swap value semantics — [spec](docs/ring_deque.md) |
| `mini::PriorityQueue<T, Container, Compare>` | [`priority_queue.hpp`](include/mini/priority_queue.hpp) | Done | Binary heap over `mini::Vector` by default, with custom comparators and linear-time heap construction — [spec](docs/priority_queue.md) |
| `mini::FlatHashMap<K, V>` | [`flat_hash_map.hpp`](include/mini/flat_hash_map.hpp) | In progress | Open addressing, linear probing, tombstones — [spec](docs/flat_hash_map.md) |
| `mini::HashMap<K, V>` | [`hash_map.hpp`](include/mini/hash_map.hpp) | Done | Separate chaining with cached hashes; references stay valid across rehashes like `std::unordered_map` — [spec](docs/hash_map.md) |
| `mini::Optional<T>` | [`optional.hpp`](include/mini/optional.hpp) | Done | In-place aligned storage |
| `mini::UniquePtr<T>` | [`unique_ptr.hpp`](include/mini/unique_ptr.hpp) | Done | Move-only ownership, `make_unique` |
| `mini::SharedPtr<T>` | [`shared_ptr.hpp`](include/mini/shared_ptr.hpp) | Stage 9 | Atomic ownership, `make_shared`, aliasing ownership, pointer casts, and custom deleters — [spec and backlog](docs/shared_ptr.md) |
| `mini::WeakPtr<T>` | [`weak_ptr.hpp`](include/mini/weak_ptr.hpp) | Stage 4 | Non-owning observation, converting views, and atomic promotion — [spec](docs/weak_ptr.md) |
| `mini::Function<R(Args...)>` | [`function.hpp`](include/mini/function.hpp) | Stage 2 | Heap-backed callable type erasure with copy/move value semantics — [spec](docs/function.md) |
| `mini::SpscQueue<T, Capacity>` | [`spsc_queue.hpp`](include/mini/spsc_queue.hpp) | Stage 1 | Bounded lock-free single-producer/single-consumer queue with raw storage |
| `mini::move` / `mini::forward` | [`move.hpp`](include/mini/move.hpp), [`forward.hpp`](include/mini/forward.hpp) | Done | Hand-written reference-collapsing utilities |

## Building and testing

Requirements: CMake ≥ 3.21 and a C++20 compiler (GCC ≥ 12, Clang ≥ 15, or
Apple Clang).

```sh
cmake --preset dev            # Debug + ASan/UBSan
cmake --build --preset dev
ctest --preset dev
```

Use the `release` preset for an optimized build without sanitizers.

Containers are built in checkpoints. Each test suite only enables the
checkpoints up to its configured stage, so work-in-progress stages do not break
the build:

```sh
cmake --preset dev -DMINI_RING_DEQUE_STAGE=3
```

| Cache variable | Default |
|---|---|
| `MINI_LIST_STAGE` | `3` |
| `MINI_RING_DEQUE_STAGE` | `3` |
| `MINI_PRIORITY_QUEUE_STAGE` | `3` |
| `MINI_SHARED_PTR_STAGE` | `9` |
| `MINI_WEAK_PTR_STAGE` | `4` |
| `MINI_FUNCTION_STAGE` | `2` |
| `MINI_SPSC_QUEUE_STAGE` | `1` |
| `MINI_FLAT_HASH_MAP_STAGE` | `0` (disabled) |
| `MINI_HASH_MAP_STAGE` | `2` |
| `MINI_STL_SANITIZERS` | `ON` |
| `MINI_STL_WARNINGS_AS_ERRORS` | `OFF` |

The build also compiles every public header in its own translation unit to
make sure each one is self-contained.

## Using it in another project

```cmake
add_subdirectory(path/to/STL)
target_link_libraries(my_target PRIVATE mini::stl)
```

```cpp
#include "mini/vector.hpp"

mini::Vector<int> v;
v.emplace_back(42);
```

## Layout

```
include/mini/   header-only library
tests/          one test executable per component
docs/           checkpoint specs, invariants, and complexity contracts
```

## License

[MIT](LICENSE)
