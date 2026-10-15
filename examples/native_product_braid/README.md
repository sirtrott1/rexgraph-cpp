# Owned three-factor Yang–Baxter witnesses

This example adopts an existing native product hexagon. It compares adjacent
signed swaps in the orders `first, second, first` and `second, first, second`.
Both finish in the same actual nested carrier with sign `(-1)^(pq+pr+qr)`.
The left-associated routes retain five and seven primitive operations; the
right-associated routes retain seven and five. Reassociation is explicit.

The witness owns both operation histories, intermediate fields, reverse paths
and exact boundary comparisons. Cell certificates retain all original flags,
including repeated, inactive and zero flags. Complete declared type maps also
cover directions without current cells. Native histories use the original
hexagon fields, and the three cutoffs always name original A/B/C sources.

```sh
cmake -S examples/native_product_braid -B build/braid -DRFC_SOURCE_DIR="$PWD"
cmake --build build/braid -j1
build/braid/native_product_braid
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
This finite strict rational CPU profile requires the supplied additional graded
interchange declaration. These signed swaps are involutions; general knot
transport, recursive braided coherence and arbitrary-map naturality remain
separate mathematical constructions.
