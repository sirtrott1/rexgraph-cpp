# Exact native system

The source remains the typed relational complex with its original cell identities, occurrence identities, aspects, order, scales, and declared empty grades. Numerical maps and learned parameters are derived objects with explicit ownership and reconstruction contracts.

The library provides sparse arithmetic, shared exact factors and finite native consumers. The separate RexLogic companion uses these operations for declared logic and electrical models.

## 1. Corrections to existing contracts

| Contract | Implementation | Regression tests |
|---|---|---|
| A low-level attaching-word homotopy certificate could accept paths or relators from another source | Validate both path owners and the selected attaching declaration, including empty certificates | `tests/topology/test_relational_homotopy.cpp` |
| `is_local_system` checked invertibility without the cochain condition | A local-system assertion now requires coherent restrictions with square-zero coboundary; the weaker invertibility predicate remains separately named | `tests/topology/test_sheaf_cohomology.cpp` |
| Chronological paths could report a stale number of contacts; sibling routines disagreed on invalid time order | Retain immutable route witnesses and use consistent contact validation | `tests/temporal/test_chronological.cpp` |
| Integral homology used rectangular integer storage and explicit inverse transformations | Store only nonzero integer entries; retain and replay unimodular operations; reconstruct only requested transform columns | `tests/numeric/test_sparse_integer_smith.cpp`, integral and branching homology regressions |
| Polynomial chain/Bianchi generators visited a rectangular coordinate product | Store polynomial entries sparsely and join the actual adjacent support | `tests/geometry/test_algebraic_locus.cpp` |
| General metric sparse actions converted through complete coordinate fields | Use a shared sparse, square-root-free LDL certificate and sparse apply/solve frontiers; require explicit sparse callbacks for external metrics | `tests/numeric/test_positive_semidefinite.cpp`, `tests/field/test_sparse_metric_actions.cpp` |
| Prepared selected solves could allocate complete numerical right-hand sides | Sparse primal/transpose solves and sparse selected dual witnesses reuse the same immutable exact factors | `tests/numeric/test_sparse_prepared_solve.cpp` |
| Complete frame preparation retained a row container for every declared row | Reduce only represented temporary rows, retaining the original map and all output dimensions | `tests/numeric/test_sparse_exact_frames.cpp` |
| Graded fitting required an already-flat baseline even when the editable entries could close it | Keep the fixed adjacent defect as an affine right-hand side and solve the full closure condition | `tests/learning/test_affine_transport_completion.cpp` |
| Some native tower actions rejected declared empty grades | Preserve those grades and their actual carrier/metric declarations | `tests/inference/test_native_factored_calculus.cpp` |

The arithmetic tests include carriers with billions of declared coordinates and only a few stored values. Such a declaration does not allocate a Cartesian numerical array. A request for a *complete* kernel, all coordinates, or every selected result must still admit the corresponding actual output family. Exact elimination may create additional nonzero entries; the implementation retains and checks that fill instead of rounding it away.

## 2. Shared execution objects

`numeric::PreparedPositiveSemidefinite` supplies a reusable exact PSD certificate. `field::ExactMetric` reuses its positive factors for a general rational form; its identity and diagonal forms retain their simpler actions. Sparse blocks can contain enormous implicit identity carriers. External metrics must explicitly supply the sparse operations required by the caller.

`numeric::PreparedExactSolve` owns the original sparse equation, its permutations and exact elimination factors. Primal, transpose and selected-output solves validate the original equation. `numeric::PreparedExactFrames` retains

\[
A=F C,\qquad A K=0,
\]

with the complete image and kernel families. These basis coordinates do not replace native source identities.

`field::PreparedOperatorFamily` and `field::FactoredAction` compose source boundary, adjoint and prepared global operations. The native wrappers retain real publication pins, carrier declarations, availability and execution authority. Stored native fields keep sparse values. A full-coordinate realization is an explicit request; internal composition and selected contractions use sparse ports.

The implementation distinguishes three kinds of ownership:

1. An immutable relational source and its declared metric/fiber structures.
2. A genuine frozen publication, including its actual field and operator pins.
3. A prospective derived object with its source dependencies and exact witness.

A derived source is not assigned a fictitious publication identity. The native batch attachment wrapper, for example, retains the old publication and the separately staged new source. Publishing that source is a separate operation.

## 3. Native Hodge, Green, Dirac, and temporal calculus

The native operator family supports sparse forward, transpose and selected actions. Prepared Hodge frames are shared by decomposition, assertion diagnostics, sector calculations and batch updates. Their Gram solves are reused.

For an original grade field, Hodge decomposition retains

\[
x=x_{\mathrm{grad}}+x_{\mathrm{curl}}+x_{\mathrm{harm}}.
\]

The chain condition, exact metric orthogonality and reconstruction are checked separately. A graph cycle is not silently filled with a higher relation. The complete lower potential kernel is retained; an arbitrary single anchor is not substituted for that kernel on a branching source.

The new native source Green adapter adopts source-factor operator plans. For two actual endpoints it verifies

\[
g'-Jg=G'\big((I'K-JI)u+I'(u'-Ku)-\lambda(L'J-JL)g\big).
\]

The three contributions are realization change, source-field innovation, and operator change. The adapter retains the old and new source/field owners and the declared correspondences. It does not require an explicitly expanded Hodge operator as an input.

Native tower plans provide Dirac, anti-Dirac, and declared exact Cayley actions. Their rational finite transformations are checked against their original equations. They preserve the appropriate declared quadratic form where the construction requires it; they do not claim to be the continuous-time exponential.

Selected native moments, operation words, sector transports, finite interactions and ensemble contractions carry their real dependencies and cutoffs. No unrequested full covariance operator, composite response map or family of all output coordinates is needed for a selected scalar contraction. A user who explicitly asks for all subset interactions is still asking for an exponentially sized output.

Prepared invariant consumers reuse Hodge frames and reduced factors for determinant, volume and harmonic-completion identities. No eigenvalue list or eigenbasis is required.

## 4. Learning retains conditions and full parameter families

The graded transport learner now solves the affine closure problem on one selected grade with the adjacent grades fixed. Its fixed contribution need not already be flat if the declared editable parameters can repair the adjacent chain equation. A defect in an unrelated fixed interface remains inadmissible.

The result is retained as

\[
\theta=\theta_0+N z,
\]

including all free directions. It is not reduced to one arbitrary solution. Observations and claims retain their original indices and evidence; the existing claim machinery distinguishes forced, excluded and open statements.

`learning::PreparedTransportFamilyGluing` compares complete learned families over explicit retained-source maps and reconstructed common stalk inputs. It supports higher selected grades, mixed selected grades, and rectangular common views. Unshared directions stay free. It retains the interface equations and an exact contradiction witness if individually feasible conditions are jointly inconsistent.

The sparse interface construction visits represented input/output support. It omits identically zero equations while retaining every original occurrence witness and declared carrier dimension. In particular, padding a common carrier does not create a rectangular table of zero equations.

`learning::PreparedAssertionDiagnosis` packages the companion's linear assertion results. It returns the Hodge components, minimum potential with the complete gauge, residual, contradiction witness, requested corruption signatures and conditional repairs. For a unit metric, an assertion vector is consistent when

\[
B^T\phi=c.
\]

For a metric assertion field, the relation is \(y=B^\dagger\phi\); the covector entry point performs the declared conversion. Signature collinearity exposes indistinguishable corruption locations. A nonzero residual proves incompatibility with the declared assertion model; it does not automatically identify which observation is wrong.

`learning::PreparedMultiplicativeRelations` requires one nonzero signed `-1/+1` incidence pair per grade-one relation and nonzero rational ratios, retaining any other declared zero occurrences. It directly solves the retained pairwise equations

\[
x_h=r_e x_t,\qquad r_e\in\mathbb{Q}^{\times}.
\]

Every consistent connected component retains one amplitude. A component with inconsistent cycle ratios permits only zero. Original relation keys, forest paths and cycle holonomies remain available. No logarithm or floating embedding is introduced. A higher twisted chain condition remains a separate requirement.

## 5. Additional finite constructions

| Construction | Public implementation | Exact declared scope |
|---|---|---|
| Finite group/ring/field laws | `algebra/finite_algebra.hpp` | Retained finite operation evidence, functionality/totality and algebra-law checks; partial or multivalued evidence remains visible |
| Batch harmonic attachments | `core/batch_attachment_update.hpp`, `field/batch_harmonic_attachment.hpp`, native wrapper | Stage all declared attachments once; reuse old Hodge frames; compute independent harmonic residuals and exact new upper-cycle witnesses |
| Cyclotomic fields, companion §§22–25 | `numeric/algebraic_field.hpp` and archive codec | Exact cyclotomic irreducibility certificate, including cases not admitted by the earlier two certificate schemes; legacy certificate encodings remain valid |
| Cyclic equivariant chains and based torsion | `topology/equivariant_torsion.hpp` | Full retained voltage declarations in all grades, exact group-ring chain checks, character specialization, based acyclic torsion and declared unit/Galois orbit |
| Integral periods and finite Jacobian data, companion §§26–27 | `field/period_lattice.hpp`, `field/finite_jacobian.hpp` | Primitive complete integral homology declaration, retained cellular cup terms, exact harmonic period pairing, supplied and checked normalization/complex structure, finite points and declared factor lattices |
| Carry/radix invariants, companion §21 and Example 25.3 | `local/radix_carry.hpp` | Rational-base carry invariants; normalization for integer bases of absolute value at least two and Gaussian base `-1+i`, within fixed declared carrier capacity |

The period construction needs oriented cup data in addition to chain incidence. Algebraic normalization needs a declared field and selected root. Period inputs retain coordinated integral cell coordinates. Supplied rational rank-two factor planes are verified and their integer spans are saturated; the consumer does not discover eigenvectors or a spectrum. Injectivity is checked for the explicitly supplied finite list of points. The finite period torus is not automatically identified with a smooth curve or its Jacobian.

The torsion consumer supplies an exact invariant for its declared based acyclic chain profile. It is not a universal manifold classification algorithm. Carry normalization does not invent additional storage positions when a declared representation is too short.

## 6. RexLogic and electrodynamics use the same mathematics

The companion's passive rational time-step implementation has moved into `field/passive_dynamics.hpp`. The old companion API aliases this native implementation. Its equation is

\[
M\dot x=(J-R)x+B u,\quad M\succ0,\quad J^T=-J,\quad R\succeq0.
\]

Midpoint and backward Euler retain exact sparse finite step relations. One prepared factor applies to actual states and inputs. The returned certificate checks the original step equation and

\[
W_{\mathrm{source}}=E_{n+1}-E_n+W_R+W_{\mathrm{step}}.
\]

The electrical interpretation is a finite declared lumped model. Charge conservation, chain closure, Boolean readout validity, temporal correspondence and energy balance are separate certificates.

The NAND instruction adapter prepares and reuses its native execution plans and workspace. One supplied control record computes all switch controls and the commanded output. The primitive relation mask is cached. Circuit events retain their actual immutable source owner.

The ordinary companion run now learns a loop repair after rewiring, glues two local higher-grade transport families, and diagnoses assertions obtained from an actual circuit event. A separate diagnostic injection is reported explicitly and does not alter the retained electrical history. Published native operator experiments reuse the circuit's declared fields and genuine publication pins.

## 7. Boundaries that remain explicit

This release does not implement a general nonlinear solver for simultaneous changes to adjacent boundary grades, a universal sheaf or homotopy discovery engine, Gröbner/ideal elimination, an automatic smooth-limit or curve-identification procedure, or a general singular holonomy-limit analyzer. A representation or exact kernel primitive is not itself such a solver.

The ordinary logic/electrical path stays in integers and rationals. Algebraic extensions are explicit separate declarations. Existing approximation modules remain separately named downstream facilities; they are not a fallback for a failed exact request.

Sparse elimination can incur fill, integer bit growth, and genuine computational hardness. Declared exact budgets can refuse an operation. The system returns supported evidence, a counterexample, a retained unresolved family, or a rejection appropriate to the request; it does not silently turn those outcomes into a numerical approximation.

## 8. Verification and reproduction

The source tree contains targeted mathematical regressions and the prior library corpus. New tests exercise positive forms, sparse general metric actions, prepared solves, huge sparse carriers, affine closure, whole-family gluing, assertion ambiguity, multiplicative cycles, batch attachment, cyclotomic fields, equivariant torsion, periods and carry laws. Independent small Fraction oracles remain confined to verification code.

The companion provides `tools/verify_unified.py` to compile and run the portable C++ corpus when CMake is unavailable. It records source/dependency hashes, compiler commands, logs, outcomes and independent oracle checks. Normal CMake/CTest remains available with all new tests registered. Optional backend and configure/install gates require their declared tools and are reported separately if unavailable.

The separate RexLogic companion contains circuit examples and its own CTest suites. Source manifests identify the exact library headers consumed by that companion.
