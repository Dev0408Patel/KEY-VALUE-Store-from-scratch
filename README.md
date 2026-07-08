# FastKV — A Lock-Free In-Memory Key-Value Store

A small, from-scratch key-value store built to deeply understand — and prove,
with benchmarks and sanitizers rather than claims — how lock-free concurrent
data structures outperform mutex-based ones under load.

> **Note on scope:** This project does not aim to compete with Redis, Memcached,
> or RocksDB. It exists to demonstrate systems-level C++ skills — memory
> management, lock-free concurrency, and performance engineering — through a
> small, fully understood, fully tested system rather than a black-box library.

---

## Table of Contents

1. [Why This Project Exists](#why-this-project-exists)
2. [What It Does](#what-it-does)
3. [Architecture](#architecture)
4. [How It Works, Step by Step](#how-it-works-step-by-step)
5. [Testing & Verification Strategy](#testing--verification-strategy)
6. [Benchmarks](#benchmarks)
7. [Build & Run](#build--run)
8. [Tech Stack](#tech-stack)
9. [Skills Demonstrated](#skills-demonstrated)
10. [Future Work](#future-work)

---

## Why This Project Exists

Production key-value stores already solve this problem well. The motivation here
was a specific gap: it's one thing to use `std::mutex` correctly, and another to
actually understand *why* lock-free structures outperform mutex-based ones under
contention — how safe memory reclamation works when readers and writers touch
the same memory concurrently, and how to prove that a "faster" implementation is
actually faster and actually correct.

The only way to close that gap honestly was to build it: implement the same
store two ways (mutex-based and lock-free), benchmark them against each other
under identical conditions, and verify correctness with the same tools real
systems teams use (ThreadSanitizer, AddressSanitizer, and a linearizability
checker) rather than "it hasn't crashed yet."

## What It Does

FastKV is an in-memory key-value store, exposed over a small TCP protocol,
supporting:

- `GET key`
- `SET key value`
- `DEL key`

It ships with **two interchangeable backends** behind the same interface:

- A **mutex-based backend** (striped locks) — the baseline.
- A **lock-free backend** — atomic CAS operations for updates, hazard pointers
  for safe memory reclamation, and no blocking on the read path.

Both are benchmarked against each other under identical multi-threaded
workloads, and both are verified for correctness under the same stress and
sanitizer suite.

## Architecture

```
Client ── TCP ──► Command Parser ──► KV Store Interface (IKVStore)
                                          │
                          ┌───────────────┴───────────────┐
                          │                                 │
                 Mutex-Striped Backend           Lock-Free Backend
                 (std::shared_mutex per shard)   (atomics + CAS + hazard ptrs)
                          │                                 │
                          └───────────────┬───────────────┘
                                          │
                                Write-Ahead Log (durability)
```

Both backends implement the same interface, so swapping between them is a
one-line change — this is what makes the performance comparison in this README
apples-to-apples rather than anecdotal.

## How It Works, Step by Step

**1. A client connects** over TCP and sends a command (`GET/SET/DEL key [value]`).

**2. The command parser** validates and dispatches the request to the active
backend through the shared `IKVStore` interface.

**3a. Mutex-based path:** the key is hashed to a shard; that shard's
`shared_mutex` is locked (shared lock for reads, exclusive for writes); the
operation runs; the lock releases.

**3b. Lock-free path:**
- *Reads* walk the bucket using atomic loads with acquire ordering — no lock is
  taken at all.
- *Writes* build a new node and attempt to swap it in with a compare-and-swap
  (CAS) loop, retrying if another thread updated the bucket first.
- *Deletes* mark the node as logically removed, but the memory isn't freed
  immediately — a reading thread might still be holding a pointer to it.

**4. Hazard pointers keep deletion safe.** Before touching a node, a thread
publishes ("hazards") the pointer in a shared registry. Before physically
freeing a deleted node, the reclaiming thread checks that registry — if any
thread still has that node hazarded, the free is deferred instead of executed
immediately. This is what prevents use-after-free without needing a lock.

**5. Writes are appended to a write-ahead log** before being acknowledged, so
the store can be replayed and rebuilt after a restart.

**6. The benchmark harness** spins up 1, 2, 4, 8, and 16 client threads running
a fixed read/write mix against both backends, recording throughput and
p50/p95/p99 latency for each configuration.

## Testing & Verification Strategy

Correctness matters more than the benchmark numbers for this kind of code —
lock-free bugs are timing-dependent and can pass thousands of runs before
surfacing. The verification approach is layered:

| Layer | Tool | What it catches |
|---|---|---|
| Functional correctness | GoogleTest | Basic logic errors |
| Data races | ThreadSanitizer | Missing/incorrect memory ordering |
| Memory errors | AddressSanitizer | Use-after-free, double-free |
| Undefined behavior | UBSan | Alignment/overflow issues |
| Memory reclamation | Custom allocation counters + Valgrind | Leaks, premature frees |
| Ordering correctness | Custom linearizability checker | Operations that "worked" but violated a valid ordering |
| Protocol robustness | libFuzzer | Malformed/malicious network input |

All of the above run in CI on every push, with the sanitizer/stress suite run
as a longer nightly job (millions of operations, 32+ concurrent threads).

## Benchmarks

*(Populate this table with your actual measured results — do not publish
placeholder numbers as if they were real.)*

| Threads | Mutex-based (ops/sec) | Lock-free (ops/sec) | Mutex p99 (ms) | Lock-free p99 (ms) |
|---|---|---|---|---|
| 1 | — | — | — | — |
| 2 | — | — | — | — |
| 4 | — | — | — | — |
| 8 | — | — | — | — |
| 16 | — | — | — | — |

Benchmarks were run with CPU affinity pinned and frequency scaling disabled to
reduce noise; each configuration was run multiple times, and the table reports
the median.

## Build & Run

```bash
git clone <repo-url>
cd fastkv
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j

# run the server
./fastkv_server --port 6380 --backend lockfree   # or --backend mutex

# run the benchmark suite
./fastkv_bench --threads 1,2,4,8,16 --backend both

# run the sanitizer-instrumented test suite
cmake .. -DSANITIZE=thread && make -j && ctest
```

## Tech Stack

- **C++20** — atomics, structured bindings
- **CMake** — build system
- **GoogleTest** — unit/integration testing
- **Boost.Asio / epoll** — networking layer
- **ThreadSanitizer / AddressSanitizer / UBSan** — correctness verification
- **libFuzzer** — protocol fuzz testing
- **Google Benchmark** — microbenchmarking
- **GitHub Actions** — CI

## Skills Demonstrated

- Atomics and explicit memory ordering (acquire/release/relaxed)
- Lock-free algorithm design (CAS loops, the ABA problem)
- Safe concurrent memory reclamation (hazard pointers)
- Custom memory allocation (slab/arena allocator, cache-line-aware layout)
- Systems programming (TCP server, epoll/event-driven I/O)
- Rigorous correctness verification (sanitizers, linearizability checking, fuzzing)
- Performance engineering methodology (controlled benchmarking, percentile
  latency, reproducibility)

## Future Work

- Sharded background compaction for the WAL
- Optional Raft-based replication for a multi-node variant
- Epoch-based reclamation as an alternative to hazard pointers, benchmarked
  against the current approach

  To check from test from root folder:

for address safety
cd build
cmake .. -DFASTKV_SANITIZE=address -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(sysctl -n hw.ncpu)
./bin/fastkv_tests
cd ..

and 

for thread safety
cd build
cmake .. -DFASTKV_SANITIZE=thread -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(sysctl -n hw.ncpu)
./bin/fastkv_tests
cd ..
