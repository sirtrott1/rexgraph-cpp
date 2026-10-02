# Native field analysis and cache

This C++20 consumer dispatches a declared rational field action through an owning
cache and shared CPU budget, changes its input
field while reusing the compiled plan, then changes an unread payload while reusing
the exact result. Returned provenance names the current publication; the original
result retains its own source.

```sh
cmake -S examples/field_cache -B build/field-cache -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/field-cache --parallel 1
./build/field-cache/field_cache
```

Link `rfc::native_field_cache` for synchronous calls, or `rfc::field_work` for
`make_cached_field_work` jobs. Include the corresponding database header.
`NativeFieldAnalysis::inspect` takes the existing authentic accession pin, field
request, realization and host policy. It reports full native input/output dimensions,
support, retained/active occurrences, scalar encoding costs and C++ output-object
bytes. GMP heap use and variable output encoding are separate costs.

`NativeFieldCache::evaluate(analysis, lease)` returns an immutable owning
`CachedNativeField`. Plan and result receipts differ: field values can change without
changing a compiled plan. Full source, carrier/declaration meanings, every downward
map including inactive maps, selected field support and the upper field-membership
catalog are retained dependencies. Empty catalogs cover future births. Numerical
equality cannot replace a receipt; exact bytes follow hash prefilters. Source sequence
and unread opaque tensors do not invalidate this action's result.

Reuse stays within one accession-directory lineage. A separately opened directory
has a different execution authority even with equal namespace and bytes. Every call
readmits its current execution lease; warm hits cannot bypass caps, cancellation or
backend availability. Results retain the requested publication and complete lower
carrier support. Historical pinned requests remain valid historical computations.

`evaluate_parallel(analysis, executor, rows_per_tile, lease)` uses the same plan
and result receipts with exclusive CPU output tiles. Serial and parallel callers
can share the same exact result. `producer_execution()` records the producer's
CPU execution separately from the request's current publication and policy.
Tile size and workspace owner control physical reuse; they do not change semantic
result identity. Every request still admits its requested execution requirements.

Concurrent calls share identical in-flight work; each running computation owns
its input and scratch. Completed parallel scratch can be retained under the
`idle_workspaces` and `idle_workspace_slots` limits. Changed fields can reuse it
with the same compiled plan and tile size. Overlapping computations take separate
loans; failed computations discard scratch after joining their workers.
A cancelled waiter leaves the producer running. Failed or
cancelled producers publish no new entries. Callbacks cannot reenter the same cache.
Keep the service alive until all calls finish; inherited live use across fork is
unsupported. CPU tiles use the existing exact ordered field kernel and per-call
joined threads; the cache creates no persistent worker pool.

Entry counts, charged descriptor bytes, result slots and pending jobs have explicit
limits. FIFO eviction and `clear()` release completed cache ownership while existing
results/pins survive. Running jobs can publish after `clear()`. These limits are
container budgets; they do not bound the process or GMP allocator. There is no
durable cache, automatic writer merge, source mutation or history deletion.
