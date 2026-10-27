# Mathematical operations

This reference describes the declared inputs, retained outputs and supported
scope of the algebraic and temporal APIs. Start with the [library overview](../../README.md)
for installation and a complete C++ example.

## Temporal calculus

### Univariate parameter domains

`geometry::PreparedUnivariateFamily` certifies complete rank strata on an owned,
bounded rational-parameter domain. Declare `UnivariateCoordinate`,
`UnivariateDomain`, source/target `UnivariateCarrier` owners and row-major
`OwnedRationalExpression` entries. Original denominator guards survive formal
cancellation: `t/t` is formally one and still excludes zero. All maximal minors
and exact Sturm isolation distinguish true rank exceptions from temporary pivot
failures; receipts retain endpoints, holes and regular cells. Algebraic point
specializations retain their family owner. This bounded finite profile provides
no implicit carrier transport across rank jumps or multivariate calculus.

### Covariant families and jets

`temporal::PreparedCovariantFamily` binds complete occurrence-level rational
families to retained source owners and certified regular cells. It constructs
source-owned first-order boundary, metric, connection, field, adjoint and Hodge
jets, exact moment derivatives with every ordered operator term, and complete
changing-map affine layer families. `PreparedCovariantJet` keeps tangent closure
separate from exact family closure; rectangular maps retain both endpoint
connections. General positive metrics use guarded native solves; symbolic moment
construction currently admits diagonal rational metrics. Explicit constant graded
basis changes transform both adjacent maps and all pairings.

### Response derivatives and ordered words

`temporal::PreparedResponseJet` reuses an original guarded factor for the exact
solve and its derivative, retaining complete moving kernel/metric gauges and
checking every original row. `DerivativeAction` and `PreparedDerivativeWord`
retain ordered factors and every derivative-position channel without forming a
word matrix or inverse. `CrossPairingJet` includes both fields and the changing
bilinear form. `SourceCellJetSelectors` transports each actual source selector
through the declared frame. Use the family factories to retain original rational
operator/scalar guards; direct declarations describe first-order point data.
Rectangular and singular gauged responses are partial solves, not total inverses.

### Rotating coefficient responses

`temporal::OwnedConsistentLinearSystem` certifies a complete, consistent,
full-column-rank real system on its actual retained source. A typed occurrence
selector feeds `PreparedRotatingCoefficient`, which reuses original-map
response factors and complete adjoint-kernel gauge. It returns exact phase-
dependent obstruction, full least-squares solution and residual under identity
metrics with no grade-two cells and positive harmonic row share. Zero obstruction,
negative formula coefficients and removable proof substitutions remain valid;
original finite-chart holes and the separate h=-1 endpoint retain their domains.

### Refinement maps and diagrams

`temporal::OwnedRefinementLevel` retains an actual source, positive grade metrics
and declared coordinate frames. `PreparedRefinementMap` keeps complete native map
terms and separate chain, metric, adjoint, Hodge, sector and Green comparisons.
Declared retractions preserve every private fine direction and all pairing cross
terms. `PreparedRefinementDiagram` checks every path in a bounded forward DAG,
retaining ordered routes, reverse/coarsening legs and complete joint innovation
channels. Limiting-contract metadata keeps supplied infinite hypotheses and
numerical observations separate from the checked finite evidence.

### Surface holonomy and inheritance

`temporal::PreparedSurfaceHolonomy` binds a certified square-tiled surface to a
primitive integral cohomology class, retaining every occurrence transport and a
whole Laurent-family closure proof. Its finite rational phase chart preserves
original domain holes; `half_turn()` declares the separate h=-1 endpoint.
`PreparedSurfaceInheritance` certifies full cup/psi duality and the complete
inherited gradient, curl and harmonic sectors under the identity-metric,
rank-one unitary surface theorem. Finite phase samples remain separate from the
limit proof. The optional coordinate jet bridge composes with covariant-jet and response-derivative actions;
it retains the formal h derivative and chart derivative as distinct quantities.

### Krylov certificates and Cayley orbits

`inference::PreparedNativeKrylov` certifies a field-specific monic minimal
polynomial using native actions, a complete recurrence and independent rank
witnesses. `PreparedNativeCayleyOrbit` reuses the native Dirac–Cayley action for
a real rational field on one declared source grade. It proves complete rational
mode coverage, combines full-vector minimal periods and preserves unresolved
modes. Factor discovery is exhaustive within its explicit work budget; exhaustion
refuses completeness. Samples retain each ordered step through an owned history.
Positive metrics, the full strict chain and actual publication availability are
required. The initial history profile admits at most 256 sampled steps.

## Integral and algebraic constructions

### Square tiled surfaces

`rfc/field/square_tiled_surface.hpp` provides
`PreparedSquareTiledSurface::prepare(right, up, subdivision, orientation, key,
revision, guard)`. Declared bijections glue the actual square sides; connectedness,
edge pairings and vertex links certify the admitted closed orientable surface.
The result retains every corner identification, ordered face flag and derived
fundamental/cup declaration. `integral_periods(guard)` uses the geometric identity
metric; an explicit native SPD metric overload is available. Its result owns the
surface, integral-period construction, complete gluing-cycle generator certificate and
integral symplectic frame. Cylinder cores remain a separate, possibly incomplete
family.

### Integral periods and path comparisons

`rfc/field/period_construction.hpp` provides
`rfc::field::PreparedIntegralPeriodConstruction`. It constructs a complete
primitive torsion-free integral H1 basis by Smith actions, metric harmonic dual
cochains, and source-owned connecting chains and period points. Supply the
fundamental cycle, occurrence-based cup terms, coordinated integral lattice
basis, source key/revision, native identity/diagonal/sparse SPD metric and an
`ExactActionGuard`. Preparation delegates to `PreparedIntegralPeriodLattice` for
full integral, harmonic, cup and higher-chain validation. `lattice()` feeds the
existing finite Jacobian APIs; no surface orientation or normalization is inferred.
`point()` uses retained present unit pairwise edges; decorated, branch, nonunit
or disconnected path requests refuse explicitly. `homology_coordinates()` and
`compare_paths()` retain and certify closed-chain changes modulo the full lattice.
Smith changes and a scoped container/scalar work receipt remain available.

### Integral symplectic frames

`rfc/field/integral_symplectic_basis.hpp` also constructs a symplectic frame for
any accepted `PreparedIntegralPeriodLattice`. Smith row actions produce primitive
partners without a bounded search for unit entries. `basis()` is the full integral
cohomology transformation T, with T^T Omega T equal to the standard form;
`cochains()` is C*T and `cycles()` is Z*T^{-transpose}. The inverse, intermediate
actions and unimodular certificate are retained. Feed the basis to the existing
finite Jacobian validator with a separately declared normalization and embedding.
Surface normalization, smooth limits and universal vertex injectivity remain
separate mathematical questions.

### Finite Jacobian normalization

`rfc/field/finite_jacobian_normalization.hpp` provides
`PreparedFiniteJacobianNormalization::prepare(symplectic, frequencies, embedding,
guard)`. Each `DeclaredPeriodFrequency` supplies an exact squared frequency and
its positive root in the actual selected real field. The constructor builds
polynomial projectors of the accepted harmonic star, proves their full coverage
and exact identities, and delegates positivity/Riemann checks to the existing
finite Jacobian validator. Distinct frequency values can have higher-dimensional
eigenspaces; those spaces remain whole. Rational and algebraic frequencies are
supported without spectral or root discovery. The result retains the original
period/symplectic owners, declarations, projectors, normalizer and native Jacobian;
`point()` maps an actual owned integral-period point. Period matrices depend on
the chosen integral symplectic frame, so comparisons require the full declared
frame transformation. Smooth limits and universal injectivity are separate.

### Elliptic factor construction

`rfc/field/elliptic_factor_construction.hpp` provides
`PreparedFiniteEllipticFactors::prepare(normalization, guard)`. On an accepted
finite-frequency normalization with rational squared frequencies and two-dimensional sectors,
it constructs each complete integral kernel of S^2+mu*I with native Smith actions.
The equation's denominator and basis changes are retained. The existing native
supplied-plane validator certifies saturated lattices, positive intersections,
exact factor moduli and the complete isogeny index. `normalization()`, `kernels()`
and `certificate()` retain the actual source/field/embedding owners and full
proof data. Nonrational frequencies or larger repeated sectors refuse this
automatic profile while their accepted normalizations and existing explicitly
supplied-plane workflows remain available. No general Jacobian decomposition,
modulus reduction or smooth/CM identification is inferred.

### Cyclic character decompositions

For a declared free cyclic group-ring chain, include
`rfc/topology/cyclic_characters.hpp` and call
`rfc::topology::PreparedCyclicCharacterDecomposition::prepare(chain, field_limits, guard)`.
The completed object retains the original chain and one primitive cyclotomic
sector for every divisor of its deck order. Its `homology_dimensions()` sums
each sector's all-grade dimensions with its certified field-degree weight.
Receiving limits and cancellation apply to the complete census. This is an
exact chain calculation; a geometric covering interpretation needs its own
declared projection and hypotheses.

### Branch chain fields

`rfc/local/branch_chain.hpp` provides `rfc::local::PreparedBranchChain` for
positive-share branch chains with explicitly ordered vertex and relation keys.
Its complete rational basis spans the grade-zero boundary-transpose kernel;
initial values extend to the full finite field. `declared_modes` checks supplied
exact algebraic ratios and original-source residuals. A partial root family
remains partial; completeness requires an independent mode for every kernel
direction. Generalized repeated-root modes are a separate profile.

## Persistence and finite search

### Declaration archives

`inference::CDeclarationArchive` reconstructs the admitted algebraic and temporal workflows from a
bounded, versioned graph of original declarations. It reuses native source,
algebraic and temporal codecs, rebuilds fresh shared dependencies and reruns the
native certificates. Ordered programs, parameter holes, roots, cup/voltage data,
refinement histories and original producer/evidence metadata remain retained.
Checksums preserve bytes; they do not restore mathematical or runtime authority.

### Finite source search

Build `rfc_finite_solve` with `RFC_BUILD_APPS=ON`. The command reads a canonical
finite-source archive and supports bounded enumeration, propagation, and
all-different matching on explicitly named grade-one relations:

```sh
rfc_finite_solve problem.rfcfs 4194304 propagated
rfc_finite_solve problem.rfcfs 4194304 distinct 0,1,2
```

Every returned witness is checked against the original domains and charts.
Work exhaustion returns `budget_exceeded` with no witness. Selecting a search
profile changes execution, while the retained source remains unchanged. The
`finite-tools` install component installs this command separately.
