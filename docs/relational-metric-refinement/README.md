# Exact metric choice within the relational complex

## Result

The C++ certificates exercise metric choice as admissible data attached to a
retained relational complex. Primary identities, occurrence multiplicities,
Boolean aspects and the chain condition remain structural. Positive rational
metrics determine exact pairings, adjoints, Hodge representatives and response
measurements. Compatible refinements and local descriptions preserve their
declared identities; incompatible choices produce exact obstructions.

The tests use the native relational builder, boundary/coface actions, sparse
fields/tensors, metric actions, Hodge actions, chain maps, local sections and
multivariate exact polynomials. No ambient spatial grid, floating arithmetic,
eigensolver or dense field conversion is used by these experiments.

## Native implementation change

`geometry::RelationalChainMap::push_field(Grade, SparseRationalView)` now
transports an exact sparse field through an existing chain realization. It
visits active source amplitudes and their retained image terms, accumulates
only reached target coordinates, and removes exact numerical cancellations.
It preserves the existing full-vector overload for callers that select it.

The sparse overload never allocates a numerical array by source or target
carrier width. Removing a zero amplitude cannot remove a retained primary cell
or its occurrence history. The new experiment tests zero input, cancellation,
wrong carrier size, every relevant boundary column, and metric pullback pairs.

## Universal local refinement certificate

For an oriented relation from `u` to `v`, choose positive rational parameters
`a` and `b` for two refined relations meeting at `z`. The native
`geometry::ExactPolynomial` stores supported multivariate monomials with exact
rational coefficients. The test constructs the polynomial

\[
P=b(a+b)(z-u)^2+a(a+b)(v-z)^2-ab(v-u)^2
  -((a+b)z-bu-av)^2.
\]

Its canonical term map is empty: `P.is_zero()` is true. This is an identity
over `Q[a,b,u,v,z]`, covering all assignments rather than sampled values.
For `a,b>0`, division by the positive product `ab(a+b)` gives

\[
\frac{(z-u)^2}{a}+\frac{(v-z)^2}{b}
=\frac{(v-u)^2}{a+b}
+\frac{((a+b)z-bu-av)^2}{ab(a+b)}.
\]

Thus the exact unique minimizing value and trace energy are

\[
z_* = \frac{bu+av}{a+b},\qquad
E(z_*)=\frac{(v-u)^2}{a+b}.
\]

The test also checks the two universal voltage-increment identities obtained
by substituting the numerator `bu+av`, and confirms that a deliberately
altered polynomial retains a nonzero residual. Positivity and nonzero
denominators are stated hypotheses of the energy interpretation.

## Native chain refinement, metric choice and coarsening

The executable fixture is a filled three-relation cycle. One relation is split
into two, producing a four-relation cycle with the same outer boundary. Its
grade counts are `(3,3,1)` before refinement and `(4,4,1)` afterward. No geometric
embedding is supplied. The retained grade-two filling makes `B1 B2=0` a
non-vacuous assertion.

The native chain map `J` includes old objects and sends the split relation to
the sum of its two replacements:

\[
J_1e=e_1+e_2,\qquad B_fJ=JB_c.
\]

Put `s=a+b`, `alpha=a/s`, `beta=b/s`. A metric-dependent native coarsening `R`
sends the new object to `beta*u + alpha*v`, and sends `e1` to `alpha*e`, `e2`
to `beta*e`. Other cells are retained. Exact certificates establish

\[
B_cR=RB_f,\qquad RJ=I.
\]

Current-space resistance metrics are

\[
M_c=\operatorname{diag}(a+b,2,3),\qquad
M_f=\operatorname{diag}(a,b,2,3).
\]

Every pair of current basis vectors is checked for

\[
\langle Jx,Jy\rangle_{M_f}=\langle x,y\rangle_{M_c}.
\]

Voltage increments use the reciprocal conductance coefficients. Harmonic
voltage extension at the new object preserves branch response, energy and
the original terminal response, with exact zero internal Kirchhoff residual.

### Orientation convention

The native boundary column for an arrow `from -> to` is `(-1,+1)`. Consequently
`B^T V` is `V_to - V_from`. The test calls `C B^T V` its gradient-oriented branch
response. Conventional passive current along the arrow is its negative. A
common sign reversal preserves all stated KCL and energy identities.

### Recorded finite coverage

Five positive rational `(a,b)` choices include integers, proper rational
coefficients and a coefficient with numerator `2^80+1`. Five voltage values
on each of the three old objects give 625 actual test states. The test runs
74 boundary or pairing coefficient checks, plus exact field and negative
controls, for **3,896 checks**.

A midpoint perturbation `delta=2/7` gives the exact positive excess energy
`(a+b)*delta^2/(a*b)`. Reusing an incorrectly scaled conductance after subdivision
instead gives an energy residual of `-1/4` in the selected example, while the
chain map still passes. This distinguishes metric inconsistency from a
structural chain defect.

## Local fields and metric gluing

The test uses `RelationalLocalCarrier` patches closed on the actual first two
relations, with `BasicSection<SparseRationalVector>` values. It explicitly
restricts the supplied sections to their patches before gluing.

It verifies:

- Agreement on the shared object produces one exact global section.
- Restricting that result recovers the local section.
- A present zero-valued stalk remains an explicit section key with no stored
  numerical amplitude.
- Conflicting overlap values and missing required overlap values are rejected.
- Positive scalar local pairing choices glue after exact coordinate pullback.
- Conflicting pairing choices retain an overlap obstruction.

For the local change `x'=t*x`, `m'=m/t^2`, with `t=3/2`, the native sparse metric
pairings verify `m'*x'^2=m*x^2`. The local metric descriptions are pulled back to
their common description before the native gluing check.

## Complete finite Hodge and Dirac certificate

The algebraic compatibility is uniform in the admissible metric. On the total
graded carrier let `B` be lowering and let `M` be the positive rational grade
pairing. With the matching adjoint `Bdag=M^-1 B^T M`, the chain axiom gives

\[
B^2=0,\qquad (B^\dagger)^2=M^{-1}(B^2)^TM=0,
\qquad (B+B^\dagger)^2=BB^\dagger+B^\dagger B=L.
\]

The inverse notation specifies the exact solve action; it does not require
forming an inverse. Likewise, `inner(Bdag a, B b)=inner(a, B^2 b)=0` gives
gradient/curl orthogonality for every admissible pairing. The following C++
checks verify the native sparse implementation on complete finite bases under
the stated metric choices.

`tests/field/test_relational_metric_choice.cpp` retains two different loop
relations whose numerical boundaries vanish, an ordinary relation between two
objects, and a filling relating the loops. Grade counts are `(2,3,1)`.

All 13 occurrences remain present, including cancellation pairs, existing
zero-valued occurrences, an absent occurrence, and signature-owned Boolean
role and provenance bits. Copies of retained identities, occurrence ownership,
coefficient scales, sparse routing and aspect words are compared after every
metric choice.

For the same input `x=(1,0,1)` the exact results are:

| Metric | `M0`; `M1`; `M2` diagonals | Curl | Harmonic | Proposed-defect quadrance |
| --- | --- | --- | --- | --- |
| Unweighted | `(1,1)`; `(1,1,1)`; `(1)` | `(1/2,1/2,0)` | `(1/2,-1/2,0)` | `2` |
| Integer weighted | `(2,3)`; `(2,5,7)`; `(11)` | `(2/7,2/7,0)` | `(5/7,-2/7,0)` | `5` |
| Rational weighted | `(1/2,4/3)`; `(3/2,7/4,5/3)`; `(2/5)` | `(6/13,6/13,0)` | `(7/13,-6/13,0)` | `11/6` |

The gradient is `(0,0,1)` for each choice. The proposed extra higher-boundary
column `(1,1,1)` has chain defect `(-1,1)` in every case. Its metric quadrance
changes; the existence of its defect does not. The strict builder rejects that
nonclosed assertion.

All 593 checks pass, including 27 adjoint basis pairs, 18 complete graded basis
columns for `D^2=L`, anti-Dirac square and anticommutation, and 81 cross-sector
orthogonality pairs. The experiment also checks full finite reconstruction,
projector identities, harmonic kernel conditions, metric solves, positivity
and four negative controls.

The fixture's one-dimensional gradient and curl image ranks are certified
against every native image column. Their projections use exact native pairings
and division by positive quadrance. This is a complete certificate for this
fixture, not a test of a general projector-construction routine. Its sparse
Dirac helper composes the native boundary and adjoint ports; it does not invoke
the full-vector core tower convenience wrapper.

## Build and integration

The main CMake build registers `rfc_relational_metric_choice` and
`rfc_relational_metric_refinement` with CTest. After configuring the ordinary
project with tests enabled:

```sh
cmake --build build --target rfc_relational_metric_choice_tests rfc_relational_metric_refinement_tests
ctest --test-dir build -R '^rfc_relational_metric_(choice|refinement)$' --output-on-failure
```

The companion's `tools/verify_metric_choice.py` builds both core certificates,
four existing chain/semantic regression suites, and the native logic/circuit
certificate. It accepts either a sibling or pinned source tree and records
complete compiler/runtime logs and source hashes.

## Structure, measurement and scope

The implementation separates retained relational structure and semantic
refinement, exact metric measurement, operations compatible with the chain
and exact defect measurement. Local section
gluing is performed on actual relational patches.

The symbolic identity is a universal finite algebraic certificate. The
complete-basis checks cover every rational field on their finite carriers for
the specified metric choices. These tests establish the exact compatibility
claims described here; an infinite completion, smooth-manifold identification
or a quantitative discrete-to-smooth limit remains a separate theorem whose
hypotheses and earlier proof must be cited.
