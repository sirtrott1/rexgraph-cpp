# Three-factor chain maps

This example sends an interval edge to two interval edges and lifts that map to
the existing three-factor products. Matrix rows name target positions and
columns name source positions. Every grade has a matrix and a declared set of
supported positions, including empty grades. `nonzero_support` declares just the
nonzero matrix positions; add positions explicitly to retain supported zeros.

`PreparedNativeProductChainMap` adopts actual source and target hexagons. Its
three stages act on original A/B/C roles and retain each contribution before
addition. Intermediate mixed coordinates name which roles have already changed;
they do not declare another relational complex. Input and final output use the
canonical owning associated fields.

`apply` returns the retained lift. `witness` additionally checks boundary,
reassociation, signed inner and block interchange, and both Yang–Baxter routes.
Every prefix map and intermediate field is retained. Boundary routes can have
different supported zero coordinates despite exact coefficient equality; the
witness records those differences rather than erasing them.

Six independently admitted endpoint cutoffs stay in original source/target
order. A result retains its actual input and both adopted native plans, and
publishes no endpoint. These are complete degree-zero rational chain maps.
Preservation of relation types and occurrence fibers requires a separately
declared structural morphism; the linear map makes no such claim.

```sh
cmake -S examples/native_product_chain_map -B build/product-map -DRFC_SOURCE_DIR="$PWD"
cmake --build build/product-map -j1
build/product-map/native_product_chain_map
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
Execution uses the finite exact CPU profile and the hexagons' explicitly declared
graded interchange. Workspace, scalar work and integer-growth limits apply to
the full action, including empty inputs and all retained witness routes. The
example explicitly admits 64 million logical slots for both retained hexagons
and their proof histories; it does not change library defaults or measure RAM.
