# RexGraphCPP

A C++20 library implementing **Relational Field Calculus and Tensor Algebra**.
The primary object is a typed relational complex with retained assertion and
occurrence identities. Boundary matrices, graph projections, and observed fields
are derived from that source.

The library provides exact rational arithmetic, composite binary declarations,
branching relations, local stalks and fields, native algebra, metric and Hodge
operators, typed accessions, and ordered temporal persistence. Structural support,
activity, zero values, and numerical cancellation remain distinct.

This software is proprietary and available for use only through the author's
personal distribution. See [LICENSE](LICENSE).

## Build and test

Requirements: CMake 3.24 or later, a C++20 compiler, pkg-config, GMP/GMPXX,
and Python 3 for tests. These commands use Ninja. SQLite development libraries
are required when the optional SQLite adapter is enabled.

```sh
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DRFC_ENABLE_SQLITE_INTERPRETATION=ON
cmake --build build/release --parallel 1
ctest --test-dir build/release --output-on-failure -j1
```

Compile sequentially first; several translation units use substantial memory.
The `dev` and `release` CMake presets provide additional local configurations.

## Use from CMake

Keep a pinned source checkout and add it to your application:

```cmake
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RFC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RFC_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(RFC_BUILD_LOCAL_TOOLS OFF CACHE BOOL "" FORCE)
set(RFC_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/rexgraphcpp rfc EXCLUDE_FROM_ALL)
target_link_libraries(your_app PRIVATE rfc::field)
```

Link the narrowest target your application needs. `rfc::core` provides retained
complexes; `rfc::field` adds field operators; `rfc::storage` adds RFCX persistence;
`rfc::database` adds native access and interpretation. `rfc::fundamentals` is the
mathematical umbrella, and `rfc::rfc` includes the full library surface.

The library can also be installed and used without its source checkout:

```sh
cmake --install build/release --prefix /your/install/prefix
```

```cmake
find_package(RFC 0.2 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE rfc::field)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix. Headers and CMake targets are
relocatable. Optional installed adapters require `COMPONENTS SQLite` or
`COMPONENTS Vulkan` in `find_package`; the base package does not load those
dependencies. A source checkout added as a subdirectory builds library targets
only by default and does not impose warning flags on its consumer.

`RFC_BUILD_LEARNING=OFF` excludes compiled learning applications, tests and
benchmarks while keeping the header API available. Build flags for adapters
without implemented kernels reject at configuration. Source guards work from
any directory; `python3 tools/check_headers.py --compiler clang++` checks all
public headers and links two opposite include orders with warnings as errors.
That complete header check also requires SQLite headers on the current Linux
profile because it includes the optional adapter.

Relations use `rfc::core::RelationalComplex`; exact boundary, metric, Hodge,
Green and Dirac actions use `rfc::field`. These canonical implementations also
serve the native execution adapters and applications.

`rfc::native_execution` provides owning, explicitly declared rational field actions
through `rfc/database/native_execution.hpp`. Prepare with immutable native accession
pins, one selected cell stalk per retained cell, and complete upper field support.
Every retained occurrence needs an explicit map or the named constant-fiber
constructor with shared declared coordinates. `execute` retains the serial GMP
reference. Use `make_workspace(operation)` and
`execute_prepared(operation, workspace)` for a compiled serial action with reusable
scratch. Both return an owned, unpublished lower field with provenance. A workspace
belongs to one prepared operation and requires an exclusive synchronous caller.
Required unavailable backends reject; CPU fallback requires an explicit policy.
Host limits admit checked container/work counts and conservative integer growth;
they do not impose a process-wide memory quota. Cancellation is checked between
synchronous phases.

`rfc::native_field_cache` provides an owning analyzer and separate compiled-plan
and exact-result caches for that rational final-field action. Exact dependency
receipts preserve full source/carrier/map/support meanings and membership, including
empty selections. Concurrent callers can share identical pure work while keeping
scratch exclusive. See the [field cache consumer](examples/field_cache/README.md)
for authority, cancellation, limits and current-publication provenance.
`evaluate_parallel` and `make_cached_field_work` connect these exact receipts to
CPU tile dispatch with bounded completed workspace reuse.

`rfc::parallel_native_field` adds exact CPU execution of that rational final-field
operation with exclusive output tiles and a declared worker budget. It preserves
the compiled contribution order within every coordinate and joins all workers
before returning or reporting failure. See the [parallel field consumer](examples/parallel_field/README.md)
for preparation, workspace ownership and resource limits.

`rfc::native_query_execution` prepares exact scalar conjunctions and numeric
aggregates against owned native publications. Declared product paths resolve once;
pure CPU tiles preserve retained key order and optional null semantics. Tensor
windows retain axis order, structural absence and supported zero, with owning
coordinate pins and sparse summaries. Preparation and each execution admit explicit
publication availability and current limits. See the
[query and tensor consumer](examples/query_tensor/README.md) for measured complete
requests and independent/dependent read jobs.

`rfc::native_locality` adds owned stalk views, selected field regions and stage
resource lifetimes. `prepare_fiber_field` retains one coefficient per default
identity incidence and preserves explicit transport maps. `retile_field` reuses
that sealed action and input, partitioning output ownership by incidence work
and object traffic targets. Observed cache sizes can guide those targets;
they do not establish cache residency or bound GMP allocations.

`NativeFieldVectorView` keeps declared carrier, shape, support and publication
ownership with each numerical span. `PreparedNativeFieldRegion::select` computes
source-ordered selected stalks with every incoming occurrence, including cofaces
outside a patch. The full source witness remains retained. Owned linear sections
provide separate contribution summation and compatible gluing. Owned semantic
boundary actions validate declared realization maps and preserve the complete
fine residual when a checked retraction is supplied. The native semantic adapter
admits macro and micro publication cutoffs separately.
That native semantic profile requires an explicit static mathematical map
declaration. Fitted or time-varying maps need a separate availability binding.

`local/interior_interface_solve.hpp` prepares exact sparse interior and interface
solves for declared square operators. It retains coupled interfaces and
reconstructs every requested coordinate. Singular interior blocks reject this
profile. Current arithmetic limits apply to reused factors; elimination can fill
in. These solves do not infer an inverse, metric or chain boundary from a partition.

The [work dispatch consumer](examples/work_dispatch/README.md) runs independent
field and packed jobs against one shared budget. `rfc::field_work` and
`rfc::binary_work` provide owned completions and explicit dependencies;
`rfc::vulkan_work` adds device routing when Vulkan is enabled. Packed data flow
requires declared shared carriers and complete input bindings. Native publication
cutoffs and expected completion generations prevent future or stale results from
being accepted as current work.
Vulkan work factories accept an owning `VulkanBinaryWorkspaceCache` for bounded
exclusive buffer/pipeline reuse on one device context. The consumer's `--reuse`
mode reports cold batches, warm result reuse and retained device allocations.
Its `--locality` mode compares exact layouts and partitions, then matched cached
CPU/GPU batches and independent staged regions with complete field reconstruction.

`storage/prepared_source_program.hpp` binds a declared binary program once to
pinned source bitplanes. Reuse a `BinaryExecutionWorkspace` for counts, owned
masks, or ordered word callbacks. `runtime/query_mask.hpp` provides prepared mask
intersection through the same packed executor.

`inference/native_field_execution.hpp` prepares owning boundary, metric adjoint,
and down/up/full Hodge actions on complete declared `SourceGrade` carriers. It
Its finite profile accepts named unit metrics or complete positive diagonal
native metrics. The explicitly selected `GlobalActions` profile also accepts
complete declared positive rational metrics, exact Green solves and harmonic
projectors. Metric solves reuse their exact LDL factors. Harmonic frames are
prepared once; Green uses sparse augmented equations and verifies its residual.
Negative Green parameters require a nonsingular system. Dimension, fill, work
and integer-growth admission remain explicit. These actions do not require a
Hodge matrix, inverse matrix or eigensolve.
`learning/native_metric_layer.hpp` retains original ordered finite channels and
native gain/bias fields and returns prospective values. Its original finite
channels remain the supported model profile; empty grades reject explicitly.

`learning/context_transport_hypotheses.hpp` provides
`acquire_context_transport_hypotheses` for owned grade 1 transport observations.
Supply original target rows and a complete atlas of explicitly declared common
frame permutations. The result retains complete ordered stars, original cell
and occurrence maps, evidence sample indices and entry equations. Use
`PreparedTransportParameterRelations` to certify those equations and their joint
intersections. Acquired comparisons remain prospective and preserve the source.

`learning/context_transport_rules.hpp` provides `acquire_context_transport_rules`
for independent original map entries. Supply distinct observed training rows,
separate target rows, the frame atlas and a positive minimum peer count. Complete
forced peer comparisons support a conditional transfer rule. Compatible
contradictions or unresolved training comparisons prevent admission. The result
retains every candidate, its evidence status and the selected original maps;
unobserved target equality remains a hypothesis.

`learning/transport_evidence.hpp` provides `PreparedTransportEvidence` for
original whole-field transport observations. Construct it from a prepared
transport declaration and call `append` with a nonempty ordered observation
suffix and the original action limits. Each immutable child retains the original
equations and source owner, assembles the new fields and updates its sparse
reduced rows. `particular`, `kernel`, `pivots` and `free_columns` return the
complete original parameter family. Conflicting equations reject the append.
`preparation_steps` reports the canonical complete replay cost. `update` reports
reused and newly assembled observation counts and the current guarded work.

`learning/graded_transport_fit.hpp` provides `PreparedGradedTransportFit` for
learning one original boundary grade with all adjacent transport families fixed.
Use `PreparedTransportObservations::at_grade` with complete original stalks,
explicit fixed occurrence maps and independent or named parameter coordinates.
The fit retains whole field observations and enforces every applicable chain
equation. `chain_rank`, `observation_rank` and `rank` describe the separate and
combined constraints. `particular` and `kernel` retain the complete parameter
family. `materialize` requires all free values and checks the full resulting tower.
`append` shares the fixed chain and inserts only new whole field equations.
`update` reports reused observations and actual guarded work separately from the
canonical complete preparation cost.

`learning/context_transport_views.hpp` accepts rational accession and
reconstruction maps into declared common carriers. Reconstruction after
accession must recover every original direction. `acquire_context_transport_views`
computes the exact common input image of each retained star pair and compares
its complete realized boundary responses. Witnesses retain common inputs,
original reconstructions and response shapes. Unshared inputs remain in the
original parameter family. Constant response equations require explicit
`TransportParameterSupportPolicy::IncludeConstants` when preparing certificates.

`learning/context_transport_diagrams.hpp` compares explicitly selected connected
groups of original relation rows. `acquire_context_transport_diagrams` joins
observed star correspondences only when their shared boundary maps agree in
both directions. It retains the complete row, cell and occurrence maps and the
union of entry equations for each group. Certify that union jointly with
`PreparedTransportParameterRelations`; separately feasible stars can form an
inconsistent group. Shared boundary contributions retain their own flag signs
and roles.

`learning/native_exact_trainer.hpp` provides a declared serial rational gradient
profile for half the metric squared error. It retains original ordered channels,
parameters, fixed sample order, step sizes and complete updates in a separate
native history. Event, knowledge, label, publication and fitted-parameter cutoffs
are checked separately; evaluation labels cannot update parameters. Resume
replays every exact update and checks the complete ordered history. This profile
uses no random state or momentum and does not assert predictions as source facts.

`field/exact_dirac.hpp` provides an explicit rational-complex Cayley action with
separate full real and imaginary towers. `numeric/rational_function.hpp` supplies
bounded univariate arithmetic, differentiation and sparse elimination over
`Q(t)`; poles and singular systems reject. Original domain restrictions remain
the caller's declarations. `geometry/chain_homotopy.hpp` owns explicit chain
homotopy certificates. A linear certificate does not supply an undeclared
structural cylinder morphism or a universal homotopy solver.

`storage/native_series_prefix.hpp` captures an owning history through one explicit
publication cutoff. `temporal/chronological_prefix.hpp` evaluates only the declared
forward steps. `models/native_temporal_context.hpp` requires per-source namespace
cutoffs alongside host authorization and rejects newer publications before access.
Publication order is not event time; callers must declare when inputs, labels and
fitted parameters became available. These views preserve the complete source history.

The [engine consumer](examples/engine_consumer/README.md) demonstrates a separate
application adapter. The [database preview](examples/database_preview/README.md)
provides a POSIX file and directory envelope over native histories.

`storage/native_durable_store.hpp` adds an opt-in Linux store for canonical native
history. Immutable frame batches, linked commits and a shared transaction index
publish through one synchronized HEAD. Exact ID/content/predecessor retries return
the original receipt. See the [durable history consumer](examples/durable_history/README.md)
for filesystem assumptions, unknown-outcome handling and ownership.

`rfc::concurrent_history` adds one Linux service with concurrent owning readers
and serialized writers. Authentic expected snapshots use the complete root as
their conflict domain. Frame directories and checkpoint maps share immutable
storage across snapshots. See the [concurrent history consumer](examples/concurrent_history/README.md).

## Local tools

```sh
./build/release/rfc_local doctor
./build/release/rfc_local --help
```

See the [CLI README](docs/local/README.md) for examples and supported imports.
`cmake --install build/release --prefix /your/install/path` installs the local
CLI and its READMEs. The installation supplies the relocatable `RFC` CMake package and
public headers described above.

## Scope

Version 0.2.1 is a developer preview with a changing C++ API. The runtime includes
the Lossless Integrity baseline and its owning execution, durable history,
concurrent publication, locality, and prepared query/tensor extensions.
Native codecs retain their canonical compatibility fixtures. Supported external interpretation is bounded;
SQLite ingestion admits INTEGER, TEXT, and BLOB under explicit identity and role
bindings.

Rational GPU kernels, broader storage qualification, and scalable training remain
planned. Existing backend options do not establish those
capabilities. Owning one-grade rational execution, compiled serial field actions
and reusable scratch are available.

The mathematical paper defines the semantics. New implementations must preserve
retained identities, declared carriers, exact values, and ordered history.

## Repository contents

Git tracks source, tests, compatibility fixtures, build configuration, and these
READMEs. Build output and local tooling state are ignored. Run
`python3 tools/check_public_repository.py --history` to check the public index
and locally reachable history before publishing.

The optional [hardware field consumer](examples/hardware_field/README.md) reports
observed Linux CPU, cache, RAM, cgroup and PCI facts. Coordinated worker reservations
and measured warm CPU dispatch preserve the prepared field's exact operation and
source. Unknown GPU memory, compute access and stacked-cache status remain explicit;
this inventory profile does not register a GPU kernel.

The optional [Vulkan consumer](examples/vulkan_binary/README.md) uses the compiled
`rfc::vulkan_binary` leaf for exact packed binary operations. It preserves the
ordered program and frozen source, supports joined CPU/GPU word partitions, and
measures complete warm candidates before choosing a fitting plan. Device queries
report unique memory heaps and advisory budgets. Enable `RFC_ENABLE_VULKAN`; Vulkan
1.1 headers/loader and `glslangValidator` are required. Physical GPUs are required
by default; explicit software admission is available for API/shader tests.

Native query algebra provides explicit nested value paths, three-valued boolean and
role predicates, and exact grouped aggregates retaining source membership.
`cross_native_role_join.hpp` composes ordered role occurrences across two immutable
publications using a declared participant correspondence with both exact cutoffs.
Equal keys across namespaces never infer identity. Aggregate requests specify
occurrence or unique assertion multiplicity; unsupported numeric values stay absent.
