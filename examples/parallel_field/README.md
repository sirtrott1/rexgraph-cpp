# Exact CPU field tiles

This C++20 consumer evaluates one declared rational field action with one and four
requested workers, checks both against the serial reference, then advances the
source while retaining the original prepared operation and result.

```sh
cmake -S examples/parallel_field -B build/parallel-field -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/parallel-field --parallel 1
./build/parallel-field/parallel_field
```

Link `rfc::parallel_native_field` and include `rfc/database/parallel_native_field.hpp`.
Inspect an authentic publication with `NativeFieldAnalysis::inspect`, call
`prepare_parallel_field(analysis, rows_per_tile, host_policy)`, and create a
`make_parallel_workspace(operation, lease)`. Execute with
`execute_parallel(operation, workspace, executor, lease)`.

`runtime::CpuTileExecutor({workers, worker_limit, tile_limit})` schedules explicit
pure tasks. The calling thread counts toward the per-execution worker limit. The
executor launches at most `min(workers, tiles) - 1` child threads and joins them
before returning or throwing. It creates no persistent worker pool. Distinct
executor instances have separate budgets; these are not a process-wide thread quota.
The result reports requested workers, selected worker slots, launched threads and
completed tiles; a selected slot may receive no tile on a short run.

Contiguous lower-coordinate tiles own disjoint accumulators and product temporaries.
Each coordinate retains its original compiled contribution order. Preparation owns
one immutable exact copy of its fixed field input, shared read-only by executions. Full native
support, declared coordinates/maps, repeated and inactive occurrences remain owned.
The operation admits the existing one-positive-grade rational final-field action;
packed providers, metrics, solvers and models retain their existing execution paths.
P3 cache evaluation remains serial.

Copies of a prepared operation share workspace identity. A workspace is movable,
operation-bound, retains its complete analysis/source, and is atomically exclusive; concurrent or recursive reuse rejects.
Different workspaces can execute the same immutable operation concurrently. An
executor also rejects overlapping runs. Keep workspaces/executors alive and stationary
until calls finish. Inherited live use across fork is unsupported. Diagnostic tile
callbacks may run concurrently and must be thread-safe; launch/result callbacks run
on the calling thread. Existing serial provider callback order is unchanged.

Every call checks its current host lease and cancellation. Cancellation is observed
at rows and entries; errors stop new tile claims and join existing workers before
scratch is released. Scratch may contain unpublished partial sums after failure
and is reset by the next execution. Prior results and canonical inputs stay intact.
The lowest recorded non-interruption tile error is reported after joins; which
tiles start can depend on scheduling. No partial result or cache entry is published.

Rational arithmetic uses GMP with conservative growth admission and actual result
checks. No bounded-integer wrapping or approximate fallback is added. Host counts,
scratch slots and tile/worker limits bound named containers, not GMP heap or total
process memory. Results are complete, derived fields with their authentic source
publication; historical preparations remain valid historical computations.
