# Covering homotopy base change

Certify two supplied lifts of the same declared cylinder through a complete
covering pullback. `prepare` requires the actual two covering owners, a complete
comparison of the homotopy targets, and identities for the three pairs of cone
maps. It checks every cell and original incidence flag, including inactive and
zero weight flags. A wrong initial sheet rejects even when its projection agrees.

The certificate retains the pullback's cylinder, start and finish mediators.
`witness` returns both signed prism witnesses, their projected values and seven
exact naturality residuals. Supported zero remains distinct from absence.
Sixteen publication roles retain independent temporal cutoffs and native limits.
This bounded rational CPU profile certifies supplied maps; it does not solve
homotopies or infer a topological realization.

Build against source:

```sh
cmake -S rexgraphcpp/examples/native_covering_homotopy_base_change \
  -B build/covering-base-change -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/covering-base-change -j1
build/covering-base-change/native_covering_homotopy_base_change
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
