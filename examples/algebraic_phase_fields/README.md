# Exact algebraic phases

The example selects a real root of `x² − 2` with the exact interval `(1, 2)`
and complex roots of `x² − x + 1` and `x² + x + 1` in the upper half plane.
It prints exact radical coordinates, rational actions, declared norm forms,
and the different trefoil homology dimensions at the two complex phases.
A fifth root example uses its full four dimensional rational carrier.

```sh
cmake -S rexgraphcpp/examples/algebraic_phase_fields -B build/algebraic-phase-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/algebraic-phase-example -j1
build/algebraic-phase-example/algebraic_phase_fields
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.
Field declarations require a checked irreducible reduction, shifted Eisenstein,
or exact cyclotomic certificate. `CertifiedAlgebraicField::declare_cyclotomic(n)`
constructs and certifies the complete polynomial, including orders such as 12
whose polynomial has no irreducible reduction modulo a prime. Its largest
intermediate is `x^n - 1`, admitted by the existing coefficient and work limits.
Arithmetic uses bounded exact rational coefficients. A selected root,
field involution, positive metric, and retained operation history are separate
declarations. Quadratic root selection supplies exact radical coordinates.
Higher-degree roots can be selected by the exact rational-region certificates
in [the complex root example](../complex_root_certificates/README.md). A regular
rational action alone does not select a spatial complex root.
