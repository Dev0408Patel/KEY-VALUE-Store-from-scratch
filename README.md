# FastKV — A Lock-Free In-Memory Key-Value Store

A high-performance concurrent key-value store built in C++20 to demonstrate systems-level software engineering: custom memory allocation, lock-free data structures, hazard pointer reclamation, and multi-threaded network programming.

---

## Table of Contents

1. [Why This Project Exists](#why-this-project-exists)
2. [Implemented Features](#implemented-features)
3. [Architecture](#architecture)
4. [Internal Mechanics, Step-by-Step](#internal-mechanics-step-by-step)
5. [Testing & Performance Benchmarks](#testing--performance-benchmarks)
6. [Build & Run Guide](#build--run-guide)
7. [Tech Stack](#tech-stack)
8. [Skills Demonstrated](#skills-demonstrated)
9. [Future Roadmap](#future-roadmap)

---

## Why This Project Exists

Writing concurrent C++ databases is highly challenging. The motivation here was to build a system from scratch to analyze the performance characteristics and concurrency behaviors of lock-free data structures versus traditional locked structures. 

Instead of relying on high-level libraries, FastKV implements custom low-level structures—like a thread-safe slab memory pool and a hazard-pointer reclamation registry—to compare lock-free atomic Compare-and-Swap (CAS) loops against partitioned lock-striping under identical, high-contention multi-threaded workloads.

---

## Implemented Features

* **インターフェース Interface Design**: A unified polymorphism contract (`IKVStore`) letting you hot-swap database engines.
* **Mutex-Striped Backend**: Hashes the keyspace to route operations to 16 independent shards, each guarded by a `std::shared_mutex` for concurrent read access and exclusive write access.
* **Lock-Free Backend**: Bucket lists utilizing atomic pointers and Compare-and-Swap (CAS) update loops, completely eliminating lock contention.
* **Hazard Pointers**: Safe Memory Reclamation (SMR) tracking what nodes reading threads are viewing, preventing Use-After-Free and solving the ABA problem in lock-free operations.
* **Slab Allocator**: A pre-allocated object memory pool that handles node recycling in $O(1)$ time, bypassing global allocator lock contention and ensuring CPU cache-locality.
* **POSIX TCP Server**: A native networking listener implementing thread-per-client handlers and parsing text commands (`SET`, `GET`, `DEL`, `SIZE`, `CLEAR`, `QUIT`).

---

## Architecture

```
Client ── TCP ──► Command Parser ──► KV Store Interface (IKVStore)
                                          │
                          ┌───────────────┴───────────────┐
                          │                                 │
                 Mutex-Striped Backend           Lock-Free Backend
                 (std::shared_mutex per shard)   (atomics + CAS + hazard ptrs)
                          │                                 │
                          └─────────────────────────────────┘
                                          │
                                 Slab Allocator (memory pool)
```

---

## Internal Mechanics, Step-by-Step

### 1. Networking & Parsing
A socket connection triggers a thread execution loop in `main.cpp`. The thread parses raw incoming character streams. Commands matching the protocol (e.g., `SET username John`) are parsed and dispatched down to the active `IKVStore` backend.

### 2. Mutex Shard Path
Keys are hashed using `std::hash<std::string> % 16`. 
* **Reads (`Get`)**: Acquire a shared reader lock (`std::shared_lock`), allowing parallel readers to scan the bucket chain concurrently.
* **Writes/Deletes (`Set`/`Delete`)**: Acquire an exclusive writer lock (`std::unique_lock`), blocking other operations on that specific shard map during modification.

### 3. Lock-Free Path
Operations are performed without holding any locks:
* **Writes (`Set`)**: Allocates a node block from the thread-safe **Slab Allocator**, updates the item parameters, and loops on `compare_exchange_weak` to replace or append the node to the target bucket pointer.
* **Reads (`Get`)**: Loops through the bucket's linked list using atomic loads. To prevent a concurrent deleting thread from deallocating the node mid-read, the reader registers the target node in the **Hazard Pointer Registry** using hand-over-hand protection loops before dereferencing it.
* **Deletes (`Delete`)**: Unlinks the target node using an atomic CAS pointer swap. The unlinked node is marked as *retired* and pushed to a thread-local queue. It is safely reclaimed to the Slab Allocator only when the Hazard Pointer Registry verifies that no active reading thread hazards it.

---

## Testing & Performance Benchmarks

Correctness is validated under extreme thread concurrency using a layered testing approach:

* **GoogleTest Suite**: Basic CRUD validation, clear operations, and concurrent multi-threaded stress runs.
* **ThreadSanitizer (TSan)**: Compiles the codebase to audit all atomic operations and ensure 100% data-race-free executions.
* **AddressSanitizer (ASan)**: Audits the custom Slab Allocator and Hazard Pointer registry to verify there are zero memory leaks or Use-After-Free conditions.

### Measured Latency Percentiles (Stress Benchmark Run)
Below are the actual measured transaction latencies under a high-contention workload (6 writers, 6 readers, and 2 deleters executing 14,000 concurrent operations):

| Backend | Operations | P50 (Median) | P90 | P99 (Tail Latency) |
|---|---|---|---|---|
| **Mutex-Striped (16 locks)** | 14,000 | 4.29 us | 44.33 us | 254.62 us |
| **Lock-Free + Hazard Pointers** | 14,000 | **2.37 us** | **8.62 us** | 660.04 us |

* **Analysis**: The lock-free backend is **2x faster at P50** and **5x faster at P90** than lock-striping due to the complete lack of locking overhead. However, the custom Hazard Pointer garbage collection sweeps (`Scan()`) introduce a higher tail-latency spike at **P99** when reclaiming deleted memory blocks.

---

## Build & Run Guide

Ensure you compile from the `build` directory:

```bash
# 1. Configure and Build (Release mode)
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)

# 2. Run the TCP Server (choose backend: 'lockfree' or 'mutex')
./bin/fastkv_server --backend lockfree --port 6380

# 3. Execute GoogleTest and Latency Benchmarks
./bin/fastkv_tests

# 4. Compile and Run with AddressSanitizer (Memory leaks)
rm -rf *
cmake .. -DFASTKV_SANITIZE=address -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(sysctl -n hw.ncpu)
./bin/fastkv_tests

# 5. Compile and Run with ThreadSanitizer (Data races)
rm -rf *
cmake .. -DFASTKV_SANITIZE=thread -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(sysctl -n hw.ncpu)
./bin/fastkv_tests
```

---

## Tech Stack

* **C++20**: Standard features, std::atomic, memory order fences, standard threads.
* **CMake**: Build automation script.
* **GoogleTest**: Unit test framework.
* **POSIX Sockets**: `<sys/socket.h>` TCP connection layer.
* **LLVM Sanitizers**: AddressSanitizer (ASan) & ThreadSanitizer (TSan).

---

## Skills Demonstrated

* **Lock-Free Concurrency**: CAS atomic loops, ABA avoidance, thread hazard tracking.
* **Low-Latency Memory Management**: Object pool slab allocations, cache-line locality.
* **Atomics Ordering**: Precision atomic memory operations (`seq_cst`, `relaxed`).
* **Socket Systems Programming**: Thread-per-client native POSIX networking.
* **Correctness Diagnostics**: Memory safety auditing and thread race audits under sanitizers.

---

## Future Roadmap

1. **Phase 5: Write-Ahead Log (WAL)**: Introduce append-only durability logs to disk with startup replay.
2. **Phase 6: Protocol Robustness**: Integrate libFuzzer on the socket protocol parser to enforce validation boundaries.
3. **Phase 7: Thread Sweep Script**: Build a Python benchmark wrapper to sweep client thread counts (1 to 16) with pinned CPU affinity to measure performance scaling.
