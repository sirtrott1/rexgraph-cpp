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

## Exact consumers and paper conformance

The [paper conformance guide](docs/paper-conformance.md) maps the papers to the
implemented C++ APIs, their declared hypotheses, retained certificates and
remaining scope boundaries. The same exact library components serve the native
execution adapters and the logic/electrodynamics companion.

The [exact metric-choice and refinement certificates](docs/relational-metric-refinement.md)
verify admissible metric choices on retained complexes, a universal rational
subdivision identity, sparse chain-map transport, and compatible local field
and metric gluing.

- [Transport family gluing](examples/transport_family_gluing/README.md) combines
  affine boundary completion with complete parameter families on retained
  interfaces. [Assertion diagnosis](include/rfc/learning/assertion_diagnosis.hpp)
  supplies consistency, corruption ambiguity and repair with the full potential
  gauge; [multiplicative relations](include/rfc/learning/multiplicative_relations.hpp)
  retain exact equation witnesses.
- [Batch harmonic attachment](examples/batch_harmonic_attachment/README.md)
  stages actual source cells, reuses prepared Hodge frames and retains new
  upper-cycle witnesses. [Native factored actions](include/rfc/inference/native_factored_action.hpp),
  [tower operators](include/rfc/inference/native_tower_operators.hpp) and
  [selected moments](include/rfc/inference/native_moments.hpp), including
  [multitime moment changes](include/rfc/inference/native_multitime_moments.hpp),
  compose sparse exact actions with their original owners and execution limits.
- [Finite algebra](examples/finite_algebra/README.md),
  [radix carries](examples/radix_carry/README.md) and
  [algebraic phase fields](examples/algebraic_phase_fields/README.md) provide
  retained law certificates, invariant-preserving carries and checked algebraic
  field/phase declarations, including bounded cyclotomic construction.
- [Integral periods](include/rfc/field/period_lattice.hpp),
  [finite Jacobians](include/rfc/field/finite_jacobian.hpp) and
  [equivariant torsion](include/rfc/topology/equivariant_torsion.hpp) consume
  declared integral cycles, cup data, coordinate bases and attaching evidence.

The examples include standalone CMake projects. The main build registers their
demo targets under `RFC_BUILD_APPS`; the transport gluing example and learning
regressions also respect `RFC_BUILD_LEARNING`.

## Temporal product demonstration

The [temporal walkthrough](examples/temporal_dynamics/README.md) follows one
evolving incident through native database publications, historical queries,
signed span boundaries, exact field responses, finite attribution and tensor
products. It includes an offline presentation, a runnable query notebook,
independent checks of historical isolation and scoped PageRank, TextRank and
group variation comparisons.

From the directory containing `rexgraphcpp/`:

```sh
python3 rexgraphcpp/examples/temporal_dynamics/run.py --verify --package --serve
```

First compilation has visible progress. Every run preserves previous output and
creates a new directory under `build/temporal-dynamics-demo/`. The local server
enables exact queries against the selected native publication.

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
and down/up/full Hodge actions on complete declared `SourceGrade` carriers.
Its finite profile accepts named unit metrics or complete positive diagonal
native metrics. The explicitly selected `GlobalActions` profile also accepts
complete declared positive rational metrics, exact Green solves, harmonic
projectors, harmonic-complement inverses and gradient potentials. Metric solves reuse their exact LDL factors. Harmonic frames are
prepared once; Green uses sparse augmented equations and verifies its residual.
Negative Green parameters require a nonsingular system. Dimension, fill, work
and integer-growth admission remain explicit. These actions do not require a
Hodge matrix, inverse matrix or eigensolve.

For repeated global requests, retain the action returned by
`PreparedNativeExactTower::prepare_action(operation, lease)`, then call
`action.execute(field_pin, lease)` for each input. It owns the original publication,
grade, metric and parameter, together with one augmented or Gram factorization.
Applications use independent scratch and recheck the current admission limits and
cancellation. An input from another publication requires a separately prepared
action, even when carrier keys or values match. Result fields are not cached.
At the field layer, `PreparedExactExecutionTower::prepare_action` supplies the same
reusable solve with explicit `ExactActionLimits`. It accepts owned unit, diagonal
or sparse positive metrics; external metric callbacks keep their one-shot API.
The [standalone example](examples/prepared_global_actions/README.md) prepares one
action of each kind and applies it to two retained fields. `HarmonicInverse`
returns `L_k^# x` with zero complete harmonic component. `GradientPotential`
returns `B_k L_k^# x` on the declared lower-grade carrier, orthogonal to the entire
kernel of `B_k^dagger`; applying that adjoint returns the gradient component.
Inverse and potential requests require zero `lambda`. Grade zero has no lower
potential carrier. A vertex anchor does not replace the full gauge for branching
relations. Field actions expose `output_grade()` and `output_size()`; native
results retain their actual output carrier in provenance.

For explicit coordinates of an owned square rational system,
`numeric::PreparedExactSolve::prepare_selected(coordinates)` prepares reusable
dual certificates. Applications return those coordinates in request order,
including repeats, and check the complete operator, input and certificate
residuals. Complete requests use the full primal solve. `apply_transpose` solves
the algebraic transpose; it does not infer a metric adjoint. The solver indexes
actual pivot-column support and reports exact preparation work, fill and bit
statistics. The [selected-solve example](examples/selected_exact_solve/README.md)
uses the original boundary of a retained complex. Native field actions keep
their complete declared outputs and carrier ownership.

`PreparedExactSolve::prepare_ordered(matrix, ordering)` accepts complete row and
column bijections while retaining the original operator. All outputs and
residuals use the original coordinates. The optional deterministic
`minimum_degree_ordering` plans from numerical support; its work and storage
are charged separately, and improvement depends on the operator. The default
pivot rule remains unchanged. An owning `factor_receipt()` exposes the exact
pivots and coordinate/swap signs; its bounded `determinant()` reuses those
factors only when requested. The [ordered-solve example](examples/ordered_exact_solve/README.md)
measures elimination storage on a retained complex's boundary Green system.

For a declared rational update `A + U C V^T`, retain
`field::PreparedWoodburySolve(A, U, C, V)` or pass an existing
`numeric::PreparedExactSolve` as its base. It prepares the complete update
directions once, requires invertible `C`, and checks the full updated equation
on every `apply`. The correction preserves the order of noncommuting factors
and stores no inverse matrix. `temporal::PreparedGreenResponseDelta` similarly
retains both shifted operators and the complete injection/operator defects.
`PreparedObservedGreenResponseDelta` adds reusable accession work. These finite
matrix plans own their declared maps, recheck current limits, and support
independent concurrent inputs; they do not infer native endpoint identity or
transport from equal dimensions. Existing one-shot update functions use the
same prepared implementation. See the
[update example](examples/prepared_updates/README.md).

For stored operators, `inference::PreparedNativeWoodburySolve` binds the baseline,
directions, coupling, independent row map and full updated operator to one
authentic publication and their declared carriers. It verifies the whole update
identity, reuses an actual native base factor and requires complete field support
at execution. Current policy and namespace/sequence cutoffs remain binding.
The [native update example](examples/native_updates/README.md) uses the paper's
head-change Gram update and retains its original source occurrences.

`inference::PreparedNativeGreenResponseDelta` owns both native endpoints,
resolving operators, source injections and complete declared ambient/source
correspondences. Its observation plan adopts the actual Green owner and both
Accession maps with an explicit output comparison. Rectangular maps retain all
directions. Separate endpoint cutoffs remain binding at preparation and each
stored-field execution. The [native Green example](examples/native_green/README.md)
reproduces the paper's three response and four observation contributions.

`inference::PreparedNativeHodgeFrames` prepares complete gradient, curl and
harmonic frames on an actual SourceGrade carrier. It validates the full declared
metric and chain family, retains exact image/kernel reductions and reuses Gram
factors for complete stored-field decompositions. Empty grades remain explicit.
Every result retains its source and basis owners with current bounds and temporal
cutoffs. The [native frame example](examples/native_frames/README.md) reconstructs
a square field before and after attachment and checks a grade-three tower.

`inference::PreparedNativeSelectedObservation` adopts an actual native solve
owner and a complete declared Accession selector. It returns original field
coordinates in requested order, including repetitions, on the declared output
carrier. Native bases also accept explicit full row/column ordering words through
`PreparedNativeWoodburySolve::prepare_base_ordered`. Ordering changes numerical
execution coordinates while source identities and attaching order remain intact.
Current bounds and explicit temporal cutoffs apply even to empty readouts.
See the [native observation example](examples/native_observations/README.md).

`inference::PreparedNativeLinearObservation` uses that same native base with
arbitrary complete rational Accession rows. It returns `S solve(A, b)` on the
actual declared output carrier, retaining zero and repeated rows. Exact equal
coefficient rows share transpose certificates when their count is below the
input dimension; larger requests use the original primal solve. Proportional
rows remain distinct. Complete maps, current resources and endpoint cutoffs
remain binding. See the
[linear observation example](examples/native_linear_observations/README.md).

`inference::PreparedNativeMetricNovelty` compares complete stored Fields on two
actual SourceGrade endpoints through a declared injective correspondence and
positive new Metric Tensor. A prepared Gram solve separates change within the
old image from its metric complement. Ordered declared cause Fields can return
the full pairing tensor, including repetition, cancellation and cross terms;
their sum must equal the innovation. Rank loss, invalid metrics, incomplete
support and foreign or later publications reject explicitly. Both preparation
and current endpoint cutoffs remain binding. The
[metric novelty example](examples/native_metric_novelty/README.md) reproduces the
paper's rectangular split and path-change pairings without assembling an
inverse or projector. Relative novelty does not establish physical birth.

`inference::PreparedNativeTensorProduct` owns binary tensor-product sources with
declared external types, ordered factor identities and complete boundary
occurrences. Exact tensor chain maps check every factor direction and act lazily
on supported product fields. Results retain both actual source owners, full maps
and input history under explicit cutoffs. See the
[native tensor product example](examples/native_tensor_product/README.md).

`PreparedNativeGluing` retains the existing atlas pushout and its complete
three-source interface square. It supports exact scalar section gluing and
boundary actions with owning inputs and explicit cutoffs.
`PreparedNativeGluingMorphism` checks complete agreeing source maps and prepares
their unique map from the pushout. See the
[native gluing example](examples/native_gluing/README.md).

Prepared native relative pairs own the actual ambient source and full
boundary-closed selections. Exact quotient actions, cycle boundary witnesses
and complete pair maps retain original occurrences and temporal ownership.
See the [native relative example](examples/native_relative/README.md).

Prepared native cylinders own the original source and canonical interval as a
declared lazy tensor product. Exact endpoint, prism and boundary actions retain
full occurrence support; signed-prism witnesses check `dH + Hd = finish - start`.
Complete strict factor maps induce owned cylinder chain maps with all four
native cutoffs checked. See the [native cylinder example](examples/native_cylinder/README.md).

Owned native coverings retain complete cell and occurrence projections, including
inactive flags, and check both local incidence stars. Exact chain pushforward and
transfer remain separate from scalar-section pullback and descent. Original flag
walks have owning lifts and fiber permutations; full deck maps and declared
pullback squares retain all endpoint witnesses and universal source maps.
See the [native covering example](examples/native_covering/README.md).

`PreparedNativeRelationalHomotopy` certifies a supplied cylinder carrier and a
complete strict morphism into an actual native target. Product types, retained
cell/occurrence bijections and both full endpoint restrictions are checked before
deriving `dK + Kd = second - first`. Results retain the structural map, original
inputs and four independent native cutoffs. See the
[relational homotopy example](examples/native_relational_homotopy/README.md).

`PreparedNativeHomotopyComposition` composes a declared homotopy with full strict
source maps before and after it. It checks the supplied domain cylinder, complete
type interpretations and all original occurrence maps, then derives the composed
endpoints and exact signed-prism identity. Results retain the original homotopy
and all eight native publication roles with independent cutoffs. See the
[homotopy composition example](examples/native_homotopy_composition/README.md).

`PreparedNativeHomotopySequence` retains repeated compositions as an immutable
ordered sequence with every intermediate cylinder, map and native publication.
Complete cumulative cell and occurrence certificates check association against
the root homotopy; equal outputs do not merge operation histories. Every retained
cutoff and native execution budget remains applicable. See the
[repeated composition example](examples/native_homotopy_sequence/README.md).

`PreparedNativeCoveringHomotopy` lifts a supplied cylinder through an owning
strict covering from an explicit initial endpoint and base comparison map.
It checks every retained incidence route and rejects shared-cell or occurrence
conflicts before deriving the lifted signed-prism action. Results own the full
lift, original inputs and six independent native publication cutoffs. See the
[covering homotopy example](examples/native_covering_homotopy/README.md).

Declared covering base changes certify both supplied lifts on every cell and
original flag, retaining three universal mediators and sixteen independent
publication cutoffs. Exact signed-prism witnesses also retain projection
residuals. See the [base-change example](examples/native_covering_homotopy_base_change/README.md).

`inference::PreparedNativeProductAssociator` checks the complete correspondence
between the two parenthesizations of three declared factors. Nested identities,
unused type pairs, ordered original flags and separate Koszul signs remain
retained. Exact field reassociation has owning round-trip and chain-naturality
witnesses, with three independent temporal cutoffs. See the
[product associator example](examples/native_product_associator/README.md).

`OwnedProductPentagon` retains all five parenthesizations of four finite strict
rational factors, complete type bijections and ordered original occurrence
fibers. Its owning witnesses check both routes, all five boundary comparisons,
and explicit reverse edges without flattening carriers or dropping zero support.
`PreparedNativeProductPentagon` binds four actual publications and independent
cutoffs; derived actions retain their input and edge history. See the
[product pentagon example](examples/native_product_pentagon/README.md).

`PreparedNativeProductInterchange` prepares distinct owned AB/BA products from
two native sources and an additional complete graded-interchange declaration.
Its exact rational swap includes `(-1)^(pq)`, retains both original boundary
words with an explicit factor-block permutation, and verifies round trip and
boundary naturality. The source cutoffs remain in the original A/B roles for
both orders. See the
[product interchange example](examples/native_product_interchange/README.md).

`PreparedNativeProductHexagon` retains twelve actual nested carriers and both
three-factor hexagons. Complete declared type maps cover unused directions;
inner swaps retain canonical interchange receipts, and block swaps preserve
the nested product. Owning witnesses keep both routes, their original flags,
reverse actions and exact boundary comparisons. All permutations retain the
original three source cutoffs. See the
[product hexagon example](examples/native_product_hexagon/README.md).

`PreparedNativeProductBraid` adopts that same owning hexagon to certify the
three-factor Yang–Baxter relation. Both adjacent-swap routes retain their
reassociations, original flags, complete type maps and exact boundary witnesses.
Canonical native fields keep every primitive input link and the original A/B/C
cutoffs. Work admission includes the numeric proof and both native routes. See
the [product braid example](examples/native_product_braid/README.md).

`PreparedNativeProductChainMap` lifts three complete rational factor chain maps
between actual owning product hexagons. It retains declared zero directions,
each intermediate contribution, and exact boundary and signed coherence
comparisons. Its six endpoint cutoffs stay independent. Linear maps do not
implicitly transport types or occurrence fibers. See the
[three-factor chain map example](examples/native_product_chain_map/README.md).

`PreparedNativeProductMapComposition` composes those maps through their actual
owning intermediate product. Factor contributions, supported-zero cancellation,
direct and staged tensor histories, and all nine endpoint roles remain retained.
Identity maps include every declared grade. The second native stage points to
its actual first-stage receipt. See the
[map composition example](examples/native_product_map_composition/README.md).

`PreparedNativeProductMapAssociativity` retains both parenthesizations of three
declared chain maps, complete triple paths and all twelve original endpoint
roles. `continue_from` adopts the actual two-map receipt while preserving its
origin, cutoffs and inherited work and integer limits. See the
[three-map example](examples/native_product_map_associativity/README.md).

`PreparedNativeExactTower::execute_batch` evaluates ordered stored fields with
an exclusive workspace from `tower().make_batch_workspace(grade, capacity)`.
The finite profile reuses rational buffers for unit and diagonal metric actions.
`PreparedNativeMetricLayer::forward_batch` uses that same workspace and retains
the original ordered channel gains and bias. Results own their field pins and
complete sample values. Scratch capacity is fixed; native work and exact integer
growth remain subject to the selected execution and accession limits.

`IntegerRelationalNetwork::forward_batch` evaluates contiguous sample columns
with an explicit `IntegerNetworkWorkspace`. Scratch capacity is fixed at
construction and reused across layers. `LatticeBoundaryOperator` provides
checked boundary and transpose actions into distinct batch buffers.

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
`parameter_knowledge_time()` returns the current fitted parameter knowledge time.

`NativeExactTrainingContract::metadata_capacity` selects the checkpoint byte
carrier size, from 8 through 65536 bytes, with a default of 4096. The complete
sample declaration and each update must fit that capacity. Resume requires the
same capacity and validates all declared samples, including later observations.

Use `make_training_workspace(sample_capacity)` with `advance_prepared` to cache
fixed channel metric pairings on a finite unit/diagonal source. The exclusive
workspace preserves every original parameter and checks availability before
reuse. `resume_prepared` replays and validates the same complete native history.
Cache slots, preparation work and coefficient growth remain subject to execution
bounds. The direct `advance` and `resume` methods remain available.

`learning/integer_perceptron.hpp` provides sparse signed integer classification.
Construct `ExactIntegerPerceptron(feature_count)` for zero parameters, or
`ExactIntegerPerceptron(weights, bias)` to restore explicit int64 parameters.
`score`, `predict` and `train_epoch` use checked integer arithmetic. Prediction
maps zero to +1; training updates nonpositive signed margins in sample order.

`field/exact_dirac.hpp` provides an explicit rational-complex Cayley action with
separate full real and imaginary towers. `numeric/rational_function.hpp` supplies
bounded univariate arithmetic, differentiation and sparse elimination over
`Q(t)`; poles and singular systems reject. Original domain restrictions remain
the caller's declarations. `geometry/chain_homotopy.hpp` owns explicit chain
homotopy certificates. A linear certificate does not supply an undeclared
structural cylinder morphism or a universal homotopy solver.

`numeric/rational_phase.hpp` supplies exact unit phases over `Q(i)` and finite
cyclic-group actions. `geometry/rational_rotation.hpp` gives coordinate-axis
rotations and rational unit-sphere charts. These use declared coordinates without
angles or trigonometric evaluation. `algebra/polygon.hpp` constructs an oriented
boundary circle, filled disk, or sphere from two oppositely attached disks;
vertex count specifies retained cells, not geometric circumference.
`topology/cyclic_attachment.hpp` owns a one-vertex grade-0..2 presentation and
its ordered Laurent-polynomial Fox contributions. Explicit common rational
transport produces owning block boundaries and their complete curvature.
Homology requires a flat specialization. This profile preserves the original
attachments and does not classify arbitrary knots.

`topology/attaching_transport.hpp` extends attachment transport to distinct,
possibly noncommuting generator maps. Its named common-coordinate constructor
owns the full source, declared maps, native stalk/fiber realization, ordered
occurrence contributions and complete boundary defect. Chronological column
steps multiply on the left. Local routes remain visible before cancellation;
flat attachments can retain nontrivial generator holonomy. The cyclic profile
uses the same ordered matrix walk.

`numeric/algebraic_field.hpp` provides exact power-basis arithmetic over a
declared rational algebraic field with a checked irreducibility certificate.
`numeric/algebraic_phase.hpp` validates a declared field involution and norm-one
units. A trace form requires the usual positivity check before metric use.
`geometry/quadratic_embedding.hpp` selects a quadratic root by an exact real
interval or complex half-plane; radical coordinates stay exact.
`topology/algebraic_attachment.hpp` retains both the formal Laurent data and its
algebraic specialization, using the existing native stalk/fiber execution through
the faithful rational carrier. Rational and algebraic homology dimensions remain
distinct. The [algebraic phase example](examples/algebraic_phase_fields/README.md)
shows an exceptional trefoil phase and a higher-degree regular action.

`temporal/phase_history.hpp` owns rational or algebraic phase operations on a
declared source/cell carrier. Prefixes retain only the requested operations;
prospective edits replay into a new history. `topology/cyclic_cover.hpp` supplies
an explicitly branched infinite cover of a retained polygon. Its integer path
lift, rather than its phase endpoint, determines winding.
`geometry/panel_assembly.hpp` declares exact panel planes and crease frames;
`temporal/fold_history.hpp` preserves shared vertices through piecewise rigid
folds and replays downstream frames after an edit. Both histories use the existing
ordered word defects and complete finite factor attribution. See the
[phase and fold example](examples/phase_fold_histories/README.md). These are logical
operation histories and exact crease kinematics; they do not assign event times
or establish material contact and collision feasibility.

`storage/phase_fold_codec.hpp` saves these original words as canonical RFCXW
archives, retaining initial data, inactive operations, cyclic lifts, and separate
prospective edits. `storage/phase_fold_durable.hpp` publishes complete revisions
through the existing Linux native store. Prefix observations exclude future steps
and proposals; restored owners are independent. See the
[phase and fold storage example](examples/phase_fold_storage/README.md).

`storage/native_series_prefix.hpp` captures an owning history through one explicit
publication cutoff. `temporal/chronological_prefix.hpp` evaluates only the declared
forward steps. `models/native_temporal_context.hpp` requires per-source namespace
cutoffs alongside host authorization and rejects newer publications before access.
Publication order is not event time; callers must declare when inputs, labels and
fitted parameters became available. These views preserve the complete source history.

`NativeTemporalSeries::append_target(endpoint, correspondences)` builds a transition
against the owned tail and checks its canonical frame and reconstructed endpoint
before publication. `append` accepts a `TemporalSegment`; `append_encoded` accepts
canonical frame bytes.

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

The [algebraic stalk consumer](examples/algebraic_modules/README.md) demonstrates
exact real-root certificates and owned field/module comparisons. Complete
algebraic occurrence blocks use the native rational fiber and prepared-field
engine, with explicit flat or deformed chain contracts.

The [cellular attachment consumer](examples/cellular_attachments/README.md) uses
distinct vertex stalks, ordered face charts and complete higher incidence maps.
It retains full loop curvature separately from chart-visible boundary defects,
validates every adjacent chain law, and records owned path and face-move receipts.

The [complex root consumer](examples/complex_root_certificates/README.md) selects
nonreal roots with finite rational contraction certificates and retains nested
observation enclosures. Native algebraic values remain exact; selected root
agreement, conjugate observations and field involutions are separate declarations.

The [algebraic storage consumer](examples/algebraic_storage/README.md) saves exact
coefficient domains, selected embeddings, stalks, complete fiber maps and typed
values in bounded canonical archives. Reopening creates fresh owners and rechecks
certificates and metric positivity. Explicit carrier comparisons and reconstruction
maps retain redundant physical directions. A Linux adapter stores ordered archive
revisions through the existing durable native history, with selected sequence reads.

The [algebraic graph consumer](examples/algebraic_graph/README.md) persists explicit
field and carrier comparisons between archives. Named references retain shared
owners without merging independently declared equal fields or stalks. Selected-root,
involution, complete measurement and reconstruction checks remain separate.

The [algebraic metric consumer](examples/algebraic_metrics/README.md) certifies
compatible real and Hermitian metrics on the full native stalk carriers. Exact
LDL factors serve reusable solves; algebraic duals, coordinate transposes and
metric adjoints stay separate. Complete retained fiber systems provide boundary,
coboundary and Hodge actions with independently checked positive pairings.

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

Exact algebraic archives can be queried through owning record pins and complete
coefficient tensor windows. See [the algebraic database consumer](examples/algebraic_database/README.md)
for selected history windows and a cached native C++ notebook example.

The [durable graph consumer](examples/graph_history/README.md) publishes complete
owner graphs with an explicit native source anchor and source/archive bindings.
Selected prefixes exclude later revisions. Shared references, independent owners,
and complete comparison receipts survive the qualified Linux store's grouped
publication, exact retries, and recovery.

The [graph database consumer](examples/graph_database/README.md) queries actual
archive owners at a selected native revision. Full grade tensors bind one vector
to every original cell stalk and contract the retained occurrence maps through
the canonical boundary action. Reversible tensor windows retain their original
cell coordinates, graph associations and endpoint pins.
