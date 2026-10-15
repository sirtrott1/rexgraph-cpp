# Concurrent native history

This Linux consumer publishes 32 native transactions while another thread reads
complete source/field/history snapshots, retains the original snapshot, and runs
full durable recovery.

```sh
cmake -S examples/concurrent_history -B build/concurrent -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/concurrent --parallel 1
./build/concurrent/concurrent_history build/example-concurrent-store
```

Use a new directory. Link `rfc::concurrent_history` and include
`rfc/storage/concurrent_native_history.hpp`. `create` and `open` return one service;
`snapshot()` returns an owning `NativeHistorySnapshot`. Pass that authentic token
to `append(transaction, expected, canonical_frames)`. A writer mutex serializes
append, status and recovery. Readers acquire one immutable complete publication
without holding the writer mutex. Atomic shared ownership is not promised lock free.

The complete expected root is the conflict domain. A stale new transaction throws
`NativeHistoryConflict`, including disjoint edits and equal output changes. There
is no automatic merge or rebase. Recompute an uncommitted request from a fresh
snapshot when appropriate. An already committed transaction ID still requires its
original predecessor and exact ordered bytes; an exact retry returns its receipt.
Whole root checks and complete replay preserve membership, declared maps/types
and chain/Bianchi validation. Numerical equality cannot identify owners or history.

An optional `std::stop_token` cancels before new head publication. Once head
replacement starts, the durable decision completes or reports an unknown outcome.
`DurableUnknownOutcome` requires `recover()` before further writes/status calls.
Readers keep the last complete publication during uncertainty or failed recovery.
That retained snapshot does not resolve a possibly newer disk transaction.

Snapshot pins and private `series()` forks outlive the service. The frame directory
uses shared radix 32 pages; append copies one path and updates exact byte counts.
Checkpoint maps are shared on append. `storage_stats()` reports physical page/path
counts separately from mathematical dimensions or validation work. Canonical
frames, retained operation order and portable archive bytes remain unchanged.

The service supports one Linux process with multiple reader/writer threads, under
the durable store's trusted local filesystem and flush assumptions. Inherited live
service use across fork rejects. Keep the service alive until all calls finish,
and keep each derived execution workspace exclusive. Reentrant reads are allowed;
same thread append/status/recovery callbacks reject. Recovery still traverses full
history. There is no disk garbage collection or history deletion.
