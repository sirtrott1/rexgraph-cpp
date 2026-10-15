# Ordered exact solves

Build from the library repository root:

```sh
cmake -S examples/ordered_exact_solve -B ../build/ordered-exact-example -G Ninja -DRFC_SOURCE_DIR="$PWD"
cmake --build ../build/ordered-exact-example -j1
../build/ordered-exact-example/ordered_exact_solve
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to
the installation prefix.

The example retains a star complex with repeated, cancelling and inactive
occurrences. Its original boundary gives the unit metric vertex Green system
`I + B B^T`. A natural solve and an explicitly ordered solve return identical
complete fields, selected coordinates and determinants.

`ExactSolveOrdering` supplies complete row and column bijections, each mapping
an ordered position to its original coordinate. `PreparedExactSolve::prepare_ordered`
owns these maps and the original operator. Primal, transpose and selected
results use the original coordinates. The retained complex and attaching order
remain unchanged. These numeric maps do not establish native carrier identity.

`minimum_degree_ordering` is a deterministic greedy heuristic on the symmetrized
numerical support. Planning work, symbolic fill and coordinate storage require
explicit admission. The default solver keeps its existing pivot rule. This
example measures a reduction in saved elimination coefficients; the upper
factor size is equal. Other operators can cost more with this ordering.

`factor_receipt()` retains the actual immutable operator and its factor owner.
It exposes both coordinate maps, the ordered pivot row slots, exact diagonal
pivots and all three permutation signs. `determinant()` multiplies those pivots
under current limits without forming an inverse or refactoring the operator.
It runs only when requested, so determinant growth cannot reject a solve that
otherwise fits its bounds. Singular operators still reject during preparation.
Receipts survive destruction or reassignment of the caller's solve handle.

Preparation, planning and application accept `ExactActionLimits`, an optional
checkpoint, or a shared `ExactActionGuard`. Reported entries and steps describe
the admitted scalar and index containers; they are not total memory or timing
measurements.
