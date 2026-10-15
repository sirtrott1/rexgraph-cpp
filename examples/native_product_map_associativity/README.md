# Three-map composition

Compose three declared rational chain maps through four actual owning
three-factor products. Both `(H G) F` and `H (G F)` retain their complete
factor and tensor paths, including declared zeros and canceled sums.

```sh
cmake -S rexgraphcpp/examples/native_product_map_associativity \
  -B build/native-product-map-associativity-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/native-product-map-associativity-example -j1
build/native-product-map-associativity-example/native_product_map_associativity
```

With an installed package, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.

`PreparedNativeProductMapAssociativity::prepare` adopts the actual F-then-G
composition and the Z-to-W map. The availability array names X, Y, Z, W,
each in its original A/B/C endpoint order. `apply` runs from an owning X field;
`witness` also retains every reassociation, signed interchange, Yang-Baxter and
boundary comparison. `continue_from` accepts a shared owning receipt produced
by that exact F-then-G plan, retains it and applies H to its actual Z image.
The example uses the latter path.

The receipt's original source remains X. It is not replaced by a native
declaration at Z. Continuation retains prior preparation/execution availability
and accounts prior work against the minimum ancestral and current scalar-work
and integer limits. Numerical diagnostics reevaluate both parenthesizations;
this is not a promise that continuation skips all diagnostic work.

The example explicitly permits 16 million logical records, 8 billion logical
rational/sparse slots and 64 million scalar steps in both its host request and
owning source authorities. These conservative bounds cover every retained
witness container; they do not allocate those capacities up front or measure
physical RAM/GMP use. Library and accession defaults remain unchanged.
This finite degree-zero rational realization supplies no structural morphism,
metric transport, publication, approximation or GPU implementation.
