# Owned native product hexagons

This example declares six owning associators, three pair interchanges and
complete inner and block type maps for twelve actual nested carriers. It checks
both three-factor hexagons: swapping a block agrees with swapping each factor
and changing association. The block remains a nested product throughout.

The right-associated block swap has sign `(-1)^(p(q+r))`; its longer route has
sign `(-1)^(pq+pr)`. The left-associated signs are `(-1)^((p+q)r)` and
`(-1)^(pr+qr)`. Each owning witness retains both routes, their intermediate
carriers, explicit reverse edges and exact boundary comparisons. Cell witnesses
retain all original flag words and their permutations, including inactive and
zero flags. Inner swaps also retain the existing canonical interchange witness
and the actual nested subproduct owners.

Build from the library source:

```sh
cmake -S examples/native_product_hexagon -B build/hexagon -DRFC_SOURCE_DIR="$PWD"
cmake --build build/hexagon -j1
build/hexagon/native_product_hexagon
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
The three cutoffs always refer to original A/B/C publications, including after
permutation. This is a finite strict exact rational CPU profile with explicitly
declared product and interchange closure. Its outputs are owning derived
observations. General map naturality, metric adjoints and GPU actions require
separate declarations and implementations.
