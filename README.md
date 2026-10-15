# RexGraphCPP

RexGraphCPP is a C++20 library for relational complexes, exact tensor fields and
versioned storage. A relation retains its identity and ordered participant
occurrences, including repeated and inactive participation. Boundary operators,
local fields and observations are constructed from that retained source.

The library provides integer and rational arithmetic, heterogeneous stalks,
declared maps, compatible gluing, metric and Hodge actions, prepared exact solves
and native queries. Structural support, a supported zero and absence remain
distinct. Exact algebraic values and explicitly selected approximation profiles
have separate APIs.

This software is proprietary and available through the author's personal
distribution. See [LICENSE](LICENSE). Third party dependencies retain their own
terms.

## Exact native consumers

The [native exact system guide](docs/native-exact-system/README.md) describes the
implemented C++ APIs, their declared hypotheses, retained certificates and
remaining scope boundaries. The same exact library components serve the native
execution adapters and the logic/electrodynamics companion.

The [exact metric-choice and refinement certificates](docs/relational-metric-refinement/README.md)
verify admissible metric choices on retained complexes, a universal rational
subdivision identity, sparse chain-map transport, and compatible local field
and metric gluing.

- [Transport family gluing](examples/transport_family_gluing/README.md) combines
  affine boundary completion with complete parameter families on retained
  interfaces. Assertion diagnosis (`include/rfc/learning/assertion_diagnosis.hpp`)
  supplies consistency, corruption ambiguity and repair with the full potential
  gauge; multiplicative relations (`include/rfc/learning/multiplicative_relations.hpp`)
  retain exact equation witnesses.
- [Batch harmonic attachment](examples/batch_harmonic_attachment/README.md)
  stages actual source cells, reuses prepared Hodge frames and retains new
  upper-cycle witnesses. Native factored actions (`include/rfc/inference/native_factored_action.hpp`),
  tower operators (`include/rfc/inference/native_tower_operators.hpp`) and
  selected moments (`include/rfc/inference/native_moments.hpp`), including
  multitime moment changes (`include/rfc/inference/native_multitime_moments.hpp`),
  compose sparse exact actions with their original owners and execution limits.
- [Finite algebra](examples/finite_algebra/README.md),
  [radix carries](examples/radix_carry/README.md) and
  [algebraic phase fields](examples/algebraic_phase_fields/README.md) provide
  retained law certificates, invariant-preserving carries and checked algebraic
  field/phase declarations, including bounded cyclotomic construction.
- Integral periods (`include/rfc/field/period_lattice.hpp`),
  finite Jacobians (`include/rfc/field/finite_jacobian.hpp`) and
  equivariant torsion (`include/rfc/topology/equivariant_torsion.hpp`) consume
  declared integral cycles, cup data, coordinate bases and attaching evidence.

The examples include standalone CMake projects. The main build registers their
demo targets under `RFC_BUILD_APPS`; the transport gluing example and learning
regressions also respect `RFC_BUILD_LEARNING`.

## Installation


The SDK requires CMake 3.24 or later, a C++20 compiler, pkg-config and the GMP and
GMPXX development libraries. The commands below also use Ninja. Run them from
the library source directory:

```sh
cmake -S . -B /tmp/rexgraphcpp-sdk-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRFC_BUILD_APPS=OFF -DRFC_BUILD_LOCAL_TOOLS=OFF \
  -DRFC_BUILD_TESTS=OFF -DRFC_BUILD_BENCHMARKS=OFF \
  -DRFC_BUILD_LEARNING=OFF
cmake --build /tmp/rexgraphcpp-sdk-build --parallel 1
cmake --install /tmp/rexgraphcpp-sdk-build --prefix /tmp/rexgraphcpp-sdk
```

This installs the public headers, relocatable CMake targets, license and user
guides. The mathematical targets are supplied as headers. Compiled tools and optional
adapters are enabled separately. The installed guides are under
`share/doc/rfc`; their example commands refer to a supplied source checkout.

### Use from CMake

An application can use the installed SDK:

```cmake
find_package(RFC 0.2 CONFIG REQUIRED)
add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE rfc::field)
```

Pass `-DCMAKE_PREFIX_PATH=/tmp/rexgraphcpp-sdk` when configuring the application.
Link the target for the operations it uses:

| Target | Operations |
| --- | --- |
| `rfc::core` | Retained relational complexes and exact coefficients |
| `rfc::local` | Stalks, declared fiber maps and owning sections |
| `rfc::field` | Boundary, metric, Hodge, Dirac and Green actions |
| `rfc::fundamentals` | The mathematical layers through temporal operations |
| `rfc::storage` | Canonical source, field and history codecs |
| `rfc::database` | Native publications, accessions and queries |
| `rfc::inference` | Owning field programs and prepared native actions |
| `rfc::learning` | Declared parameter families and evidence updates |
| `rfc::rfc` | The complete library surface |

A pinned source checkout can also be embedded directly:

```cmake
add_subdirectory(path/to/rexgraphcpp rfc EXCLUDE_FROM_ALL)
target_link_libraries(your_app PRIVATE rfc::field)
```

Embedded builds disable applications, tools, tests, learning executables and
benchmarks by default. They preserve the consumer's language settings and do
not impose warning flags. `RFC_BUILD_LEARNING=OFF` leaves the header API
available while excluding its compiled applications, tests and benchmarks.

### Optional adapters

SQLite import requires SQLite development libraries and
`-DRFC_ENABLE_SQLITE_INTERPRETATION=ON`. An installed consumer requests it with
`find_package(RFC CONFIG REQUIRED COMPONENTS SQLite)` and links
`rfc::sqlite_interpretation`.

Exact packed binary GPU operations require Vulkan 1.1 headers and loader,
`glslangValidator` and `-DRFC_ENABLE_VULKAN=ON`. An installed consumer requests
`COMPONENTS Vulkan` and links `rfc::vulkan_binary`. Physical devices are required
by default; `RFC_VULKAN_TEST_SOFTWARE=ON` explicitly admits software devices for
tests. This adapter does not provide general rational GPU kernels.

The base CMake package does not load either optional dependency. Switches for
adapters without implemented kernels reject at configuration.

## Run an exact field example

The prepared field example constructs a square with four edges and two complete
fields in one native publication. It prepares Green, harmonic and gradient
potential actions, then reuses their exact factors for both inputs:

```sh
cmake -S examples/prepared_global_actions \
  -B /tmp/rexgraphcpp-field-example -G Ninja \
  -DCMAKE_PREFIX_PATH=/tmp/rexgraphcpp-sdk
cmake --build /tmp/rexgraphcpp-field-example --parallel 1
/tmp/rexgraphcpp-field-example/prepared_global_actions
```

The [example guide](examples/prepared_global_actions/README.md) explains its
metrics, potential gauge and admission limits. Prepared actions retain the
actual source publication, carrier and metric. Further inputs must belong to
that publication; matching dimensions or numerical values do not establish a
transport. Full requested fields remain available independently of a terminal
readout.

`LocalRelationTensor`, `GradeTensorField` and `OwnedLinearFiberSystem` retain
local tensors, whole grade fields and complete occurrence maps. Owning sections
distinguish contribution summation from compatible gluing. Relational field
programs declare their input nodes, bases, metrics and maps explicitly. Learned
parameters belong to those declared objects; evidence updates do not introduce
new relations, coordinates or interfaces implicitly.

## Examples and supported operations

Each guide describes the input declarations, executed operations, retained
outputs and scope, with build commands and the supported source or SDK inputs
for that example.

### Fields, metrics and local solves

[Prepared global actions](examples/prepared_global_actions/README.md),
[selected exact solves](examples/selected_exact_solve/README.md) and
[ordered exact solves](examples/ordered_exact_solve/README.md) cover reusable
factors and requested outputs. [Prepared updates](examples/prepared_updates/README.md)
and [native updates](examples/native_updates/README.md) retain their original
source and update declarations.

[Native Green actions](examples/native_green/README.md),
[Hodge frames](examples/native_frames/README.md),
[observations](examples/native_observations/README.md),
[linear observations](examples/native_linear_observations/README.md) and
[metric novelty](examples/native_metric_novelty/README.md) explain the distinct
metric, gauge and observation contracts. [Local gluing](examples/native_gluing/README.md)
and [relative fields](examples/native_relative/README.md) retain the requested
interfaces and reconstruction.

### Products, coverings and homotopies

[Tensor products](examples/native_tensor_product/README.md),
[associators](examples/native_product_associator/README.md),
[pentagons](examples/native_product_pentagon/README.md),
[interchange](examples/native_product_interchange/README.md),
[hexagons](examples/native_product_hexagon/README.md) and
[braids](examples/native_product_braid/README.md) preserve nested carriers,
ordered occurrences and exact coherence witnesses. The
[chain map](examples/native_product_chain_map/README.md),
[map composition](examples/native_product_map_composition/README.md) and
[map associativity](examples/native_product_map_associativity/README.md)
examples retain complete factor maps and intermediate histories.

[Cylinders](examples/native_cylinder/README.md),
[coverings](examples/native_covering/README.md),
[relational homotopies](examples/native_relational_homotopy/README.md),
[homotopy composition](examples/native_homotopy_composition/README.md),
[repeated composition](examples/native_homotopy_sequence/README.md),
[covering lifts](examples/native_covering_homotopy/README.md) and
[covering base changes](examples/native_covering_homotopy_base_change/README.md)
check declared source maps and retain each publication's independent cutoff.

### Algebraic values and attaching maps

[Algebraic modules](examples/algebraic_modules/README.md),
[complex root certificates](examples/complex_root_certificates/README.md) and
[algebraic metrics](examples/algebraic_metrics/README.md) use exact declared
coefficient domains, embeddings and positive pairings.
[Cellular attachments](examples/cellular_attachments/README.md) and
[attaching transport](examples/attaching_transport/README.md) preserve original
charts and incidence maps. [Native phase shapes](examples/native_phase_shapes/README.md)
and [algebraic phase fields](examples/algebraic_phase_fields/README.md)
describe the available phase declarations.

### Storage, queries and history

The [database example](examples/database_preview/README.md) introduces native
publications. [Query tensor windows](examples/query_tensor/README.md) retain
source coordinates and endpoint pins. [Durable history](examples/durable_history/README.md)
and [concurrent history](examples/concurrent_history/README.md) document the
Linux filesystem, publication, retry and recovery contracts.

[Algebraic storage](examples/algebraic_storage/README.md),
[algebraic graphs](examples/algebraic_graph/README.md),
[algebraic database queries](examples/algebraic_database/README.md),
[phase fold storage](examples/phase_fold_storage/README.md),
[phase fold history](examples/phase_fold_histories/README.md),
[graph history](examples/graph_history/README.md) and
[graph database queries](examples/graph_database/README.md) retain owner
references, complete declarations and selected revisions.

Canonical codecs check their declared record, byte and arithmetic bounds.
Shared packets retain the complete ordered logical byte image while reusing
identical stored chunks. Segmented readers avoid expanding the entire packet;
requested contiguous leaves have explicit materialization limits. Byte sharing
does not establish source identity or transport, and these bounds are not a
total process memory guarantee.

### Execution and resource use

[Field caches](examples/field_cache/README.md),
[parallel fields](examples/parallel_field/README.md),
[work dispatch](examples/work_dispatch/README.md) and
[hardware inventory](examples/hardware_field/README.md) cover owning jobs,
explicit dependencies, reusable workspaces and shared resource admission.
The [Vulkan example](examples/vulkan_binary/README.md) covers exact packed
operations and declared CPU/GPU partitions. The
[engine consumer](examples/engine_consumer/README.md) embeds a pinned library
source checkout in a standalone application.

### Local queries and temporal walkthroughs

Enable `RFC_BUILD_LOCAL_TOOLS=ON` to build and install `rfc_local`.
The [CLI guide](docs/local/README.md) covers native snapshots, exact queries,
ordered histories and bounded SQLite import.

The [temporal dynamics walkthrough](examples/temporal_dynamics/README.md) and
[temporal relations walkthrough](examples/temporal_relations/README.md) combine
retained source changes, historical queries and exact field observations.
Their guides give the build, local server and verification commands.

## Development and validation

Python 3 is required for the independent checks. To build the default tools,
examples and regression tests from the source directory:

```sh
cmake -S . -B /tmp/rexgraphcpp-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rexgraphcpp-tests --parallel 1
ctest --test-dir /tmp/rexgraphcpp-tests --output-on-failure -j1
```

Start with one compiler process; exact arithmetic translation units can use
substantial memory. SQLite tests require the optional adapter above. The
`dev` and `release` presets are also available for local development.

`python3 tools/check_headers.py --compiler clang++` checks public headers
independently and together with strict warnings. The full header check also
requires SQLite headers because it includes that adapter.
`python3 tests/audit/check_build_contracts.py` checks embedded defaults,
installed dependencies, relocated consumers and offline documentation links.
`python3 tools/check_public_repository.py --working-tree --history` checks
repository content before distribution.

Version 0.2.1 is a developer preview with a changing C++ API. Exact sparse
elimination can fill in and increase integer sizes; work and dimension limits
remain explicit. Rational finite evolution and general continuous evolution
have different arithmetic requirements. Broader service, platform and model
capabilities require their own implemented and validated profiles.

Source, tests, compatibility fixtures, build configuration and canonical
READMEs belong in this repository. Private research, reports and local run
artifacts are maintained separately.
