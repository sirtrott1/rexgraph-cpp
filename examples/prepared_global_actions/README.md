# Prepared exact field actions

This example retains a four-edge square and two complete grade-one fields in one
native publication. It prepares each exact operation once and applies it to both
fields. Green uses the boundary and metric auxiliary equations. Harmonic projection
uses the retained frame's Gram system. `HarmonicInverse` applies the metric inverse
on the harmonic complement, with zero harmonic part. `GradientPotential` returns
`B_k L_k^# x` on the declared grade below. Its metric adjoint reconstructs the
gradient component, and its gauge is orthogonal to the complete adjoint kernel.
Each operation retains its factors without forming a Hodge matrix.

Build against the library source:

```sh
cmake -S rexgraphcpp/examples/prepared_global_actions -B build/prepared-global-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/prepared-global-example -j1
build/prepared-global-example/prepared_global_actions
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH` naming
the installation. Use `GlobalActions` and complete `SourceGrade` field pins. Keep
the returned action while processing further fields from that publication. Each
call checks its current execution lease; a future or foreign publication is refused.
`preparation_steps()` reports the original preparation work, and
`retained_entries()` counts its retained rational entries. These counts do not
measure process memory or guarantee a particular running time.

Inverse and potential requests use a zero `lambda`. A potential requires grade
one or higher; the output carrier differs from the input carrier. At the field
layer, `output_grade()` and `output_size()` describe that result. The native layer
reports the exact output carrier in the result's provenance. Branching relations
can have a kernel larger than the constant line, so a single vertex anchor is
insufficient to fix the potential.
