# Durable native history

This Linux consumer creates a fresh store, appends two canonical temporal frames,
retries the first transaction after the second, and reopens the complete history.
The zero coefficients and inactive occurrence remain in the source. Equal field
outputs do not remove either operation.

```sh
cmake -S examples/durable_history -B build/durable -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/durable --parallel 1
./build/durable/durable_history build/example-store
```

Use a new store directory. The API is in `rfc/storage/native_durable_store.hpp`,
linked through `rfc::storage`. `create(path, base, namespace)` accepts a validated
native endpoint and a nonzero namespace. `append(transaction, expected_root,
frames)` accepts a nonzero 64 bit transaction ID, the exact expected root, and one
or more canonical RFCXT frames. It returns a receipt after synchronization.

Retry with the same ID, predecessor and exact ordered bytes returns the original
receipt. Different content under that ID rejects. `status(id)` reports only
committed transactions. `snapshot()` returns an owning `NativeTemporalSeries`;
its endpoint pins remain valid after the store closes.

The store writes immutable grouped frames, linked commits and a shared transaction
index, then synchronizes and replaces HEAD. An exception of type
`DurableUnknownOutcome` requires closing the store and reopening it before status
or retry. A missing or corrupt referenced object rejects; recovery never selects
an orphan or silently rolls back. Old codecs and portable snapshots are unchanged.

This profile requires a trusted local directory on one filesystem with atomic
rename, file/directory fsync, and storage that honors completed flushes. One
coordinator holds an advisory lock; calls require exclusive access. Tests inject
exceptions and process termination, rather than hardware power loss. Semantic
validation and recovery still traverse retained history. Limits bound reachable
commits, canonical history and object size; they are not a total disk quota.
Objects and abandoned staging files are retained. There is no deletion API.
