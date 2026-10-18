# Sparse temporal stalk actions and retained Tucker fields

A temporal field can change in several ways: its coefficients can evolve on
one source, its local coordinates can change, or the source itself can gain
cells and stalk directions. Those changes require different maps. Sparse stalk
actions keep the field's actual owner, while supplied tensor factors let an
application change coordinates and check reconstruction exactly.

This guide explains the two interfaces and how to compare fields across a
source change. The RexLogic Electrodynamics companion combines them in
`tests/test_temporal_relational_field.cpp`; its observations are recorded under
`results/temporal-relational/`.

## Owned sparse stalk fields

Include `rfc/field/sparse_stalk_actions.hpp`, or the fundamentals umbrella.

```cpp
using Values = rfc::field::SparseStalkValues;
using Actions = rfc::field::SparseStalkFieldActions;

Actions actions(owned_fibers, per_grade_per_cell_metrics);
auto field = actions.bind(grade, Values{{cell_index, sparse_local_value}});
auto down = actions.boundary(field);
auto up = actions.coboundary(upper_grade, lower_field);
auto defect = actions.chain_defect(face_field);
auto laplacian_action = actions.hodge(field);
auto dirac = actions.dirac(fields_in_grade_order);
auto anti_dirac = actions.dirac(fields_in_grade_order, true);
```

The field retains the actual immutable `OwnedLinearFiberSystem`. Equal shapes
or coefficients do not identify foreign owners. A field is independent of the
chosen metric action; an action instance separately owns its metric
declarations. To cross an endpoint change, apply the declared correspondence
and explicitly bind the result in the new owner.

Lowering traverses active upper stalks and their retained occurrence ranges.
Raising traverses active lower stalks and the native reverse coface index.
Default signed scalar identity maps act on numerical nonzeros without
constructing their logical diagonal. Explicit overrides are already signed
realized maps. Inactive occurrences remain retained and cannot activate an
override. Missing numerical values are zero on the retained carrier.

The supported grade metric is the orthogonal direct sum of the per cell
metrics. General coupling inside a cell is available through native sparse
`ExactMetric` apply/solve ports. Cross cell metric blocks require a separately
declared grade metric action. No whole grade boundary, Hodge, projector, or
amplitude array is assembled by this action layer.

On a strict chain, the native action verifies the usual exact Dirac/Hodge
identities. A curved realization remains available for diagnosis: the square
of Dirac retains its additional lowering square and raising square terms.
The action does not silently declare a curved fiber system to be a chain.

Work receipts count actual stalk/incidence/coefficient visits and the maximum
observed numerical support. They do not claim allocator wide memory accounting
or inspect scratch internal to an external metric callback.

## Exact supplied tensor factors

Include `rfc/field/retained_tucker.hpp`, or the fundamentals umbrella.

```cpp
using Mode = rfc::field::RetainedTuckerMode;
using Tensor = rfc::field::RetainedTuckerTensor;

// Reconstruction is ambient x core; accession is core x ambient.
Mode mode(reconstruction, accession); // certifies accession * reconstruction = I
auto encoded = Tensor::encode(source, {mode, Mode::identity(channel_extent)});
auto restored = encoded.reconstruct();
auto residual = encoded.reconstruction_residual(expected_sparse_values);
auto evolved = encoded.with_core(new_sparse_core);
auto reframed = encoded.reframe(new_declared_modes);
```

Identity modes are symbolic. The source domain and structural support remain
distinct from its numerical entries and from the declared core coordinate
space. `with_core` rejects a nonzero outside the source's retained support.
Reduced factors are admitted only when reconstruction is exact for the
supplied source. A rejected reduced encoding returns its exact sparse
reconstruction minus source witness through
`RetainedTuckerReconstructionError::residual()`.

This API certifies supplied algebraic factors, their left inverses, and the
observed reconstruction. Relation identities, owner correspondence, metric
interpretation, and the proof that a particular factor is a Hodge frame
belong to the caller. It does not fit factors spectrally or claim minimum rank.

In the companion fixture, complete native gradient/curl/harmonic frames form
the relation mode reconstruction factor. The second mode is the explicitly
retained two direction observation stalk. Selecting sector slots in the core
reconstructs the same field as the native Hodge projections. This separability
uses coherent vertex chart changes with matching metrics; arbitrary
incidence specific internal couplings need their own field factors.

## Sparse support and resource limits

A large declared carrier need not have a large numerical field. The
context view product indexes represented rows and charges their support and
index work to the exact action guard. Extending an unused part of the carrier
therefore does not require allocating a bucket for each new coordinate.

The evolving stalk regression compares a compact carrier with a
carrier with 1,000,003 coordinates that uses the same two directions. Both preserve
the same 11 occurrence witnesses and execute the same preparation work. The
Tucker regression similarly uses a logical domain of 10^18 coordinates with
only three support entries. These cases distinguish the size of the declared space
from the entries an action actually visits.

Sparse elimination can still create fill, and exact coefficients can grow in
bit size. Choose budgets for that work as well as for the initial support.
Exceeding a budget refuses the exact request; it does not authorize rounding
or dropping directions.

## Temporal comparisons and interpretation

The companion checks `boundary_new * J - J_lower * boundary_old` by native
field actions on the complete finite source basis. It also checks the
independent adjoint comparison with each endpoint's declared metric.

An exact chain isometry under relation subdivision need not intertwine the
adjoints. The experiment records that nonzero residual explicitly. A
coordinate change and an identity lineage update likewise answer different
questions, so neither correspondence is inferred from equal dimensions.

The native local learning test glues complete parameter families over declared
interfaces, reports exact directional contradictions, and extends a learned
map to a new stalk direction. The main trajectory separately glues boundary
observations/cochains by equality on overlaps. An observation merge alone
does not assert that those values form a transport compatible sheaf section.

## Focused tests

- `tests/field/test_sparse_stalk_actions.cpp` checks adjoints on complete basis
  pairings, Dirac basis columns, chart pullback, directional defect and repair,
  rectangular inclusion, inactive occurrences and large sparse stalks.
- `tests/field/test_retained_tucker.cpp` checks exact mode factors, source
  support, temporal updates and reconstruction residuals on both small tensors
  and large logical domains.
- `tests/temporal/test_evolving_stalk_gluing.cpp` checks complete learned family
  gluing, directional conflict, observation repair, stalk extension and the
  sparse context product.

Together these tests cover the separate obligations used above: a map acts on
the right source, a reconstruction recovers the supplied field, and a temporal
comparison respects the interface being claimed.
