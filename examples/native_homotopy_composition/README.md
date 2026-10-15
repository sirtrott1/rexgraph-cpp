# Native homotopy composition

Compose an owned declared homotopy `H: X ⊠ I → Y` with complete strict maps
`u: W → X` and `v: Y → Z`. The supplied `W` cylinder carrier and its complete
type and occurrence interpretation are certified before constructing
`v H (u ⊠ id)`. Its endpoints are `v f₀ u` and `v f₁ u`, and its signed prism
checks `dK + Kd = v (f₁ − f₀) u` over exact rationals.

Every map includes all cells and original occurrences. Identity maps must also
be declared explicitly. Results own the original homotopy, both cylinder
carriers, all maps and eight native publication roles; every cutoff is checked
at preparation and execution. Equal namespace numbers do not merge owners.

Build against the source tree:

```sh
cmake -S rexgraphcpp/examples/native_homotopy_composition \
  -B build/homotopy-composition-example -G Ninja \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/homotopy-composition-example -j1
build/homotopy-composition-example/native_homotopy_composition
```

For an installed RFC package, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
This bounded profile certifies supplied scalar rational source maps; it does
not solve for homotopies, recode signatures or lift homotopies through covers.
