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
Field declarations require a checked irreducible reduction or shifted Eisenstein
certificate. Arithmetic uses bounded exact rational coefficients. A selected root,
field involution, positive metric, and retained operation history are separate
declarations. Root selection currently supports quadratic fields. A regular
rational action in higher degree does not select a spatial complex root.
