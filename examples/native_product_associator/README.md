# Owned product reassociation

This consumer constructs `(A product B) product C` and `A product (B product C)`
from three selected native publications. It checks the complete declared type
correspondence and every retained cell and boundary flag, then applies the
associator to an exact rational field. The original factors stay in order.

```sh
cmake -S rexgraphcpp/examples/native_product_associator -B build/product-associator \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/product-associator -j1
build/product-associator/native_product_associator
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to its
installation prefix. The example requires C++20 and GMP.

`PreparedNativeProductAssociator::prepare` accepts three empty owning Chain
anchors, their declared factor identities, the two inner and two outer product
declarations, and a complete type bijection. Each constructor includes unused
types and the grade zero entity token. A missing or reordered factor
correspondence is rejected even when the cell population is empty.

`field` declares structural support on one associated carrier. `reassociate`
changes only the parenthesization; `boundary` uses the original occurrence fibers.
`witness` checks both the round trip and boundary naturality. `cell_witness`
returns an owning certificate with stable nested identities and the full paired
flag words, including inactive flags, supported zero and cancellation. Each flag
records its nested path, original aspect word and separate inner and outer parity.

All actions require three explicit namespace/cutoff pairs and the actual prepared
owner. Old handles retain their original publications after a directory advances.
The bounded exact CPU profile admits dimension, reference container, work,
integer growth and cancellation limits. These are logical reference counts,
not measurements of physical memory or cache performance. Reassociation does not
publish a source revision, select a metric, or infer a general tensor transport.
