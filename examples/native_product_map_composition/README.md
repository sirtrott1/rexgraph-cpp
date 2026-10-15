# Three factor map composition

`PreparedNativeProductMapComposition` composes two complete rational chain maps
through their actual owning intermediate product. For each original factor and
grade, it retains every joined pair of declared map positions and its exact
coefficient product. Their sum defines a separately named composed map. Supported
positions remain present when their coefficients cancel or either input
coefficient is zero.

`apply` returns the direct composed lift, both staged lifts and their comparison.
Each lift retains its three original A/B/C tensor stages and individual
contributions. `witness` also retains boundary, reassociation, signed inner and
block interchange, and both Yang–Baxter routes. Every primitive prefix includes
the direct and staged histories. Boundary comparisons preserve any difference
in supported zero coordinates.

The second native stage's `prior_input()` is the actual first stage receipt.
Both stages' `input()` retains the original native field; the second stage's
current numerical input is available in `lift().input()`. Numerical coherence
prefixes remain derived observations. Execution does not publish an endpoint.

Source, intermediate and target each retain their original A/B/C endpoint roles.
Their nine cutoffs, the two maps' preparation requests and the current execution
request are admitted separately. Equal midpoint keys, dimensions or values do
not identify owners. One guard covers the entire action under the minimum
integer and work limits of all nine pins and the host policy.

`identity(product, key, revision, availability)` builds a complete identity on
that actual product, including explicitly declared empty grades. Composing on
either side retains the named identity and the original map histories. The
supplied one point tensor unit factors keep their declared identities.

The example composes a map from one point to two points with a signed map from
two points to one point.
The composed matrix has no nonzeros, while its supported direction, two
contributions and zero valued output remain retained.

```sh
cmake -S examples/native_product_map_composition -B build/product-composition -DRFC_SOURCE_DIR="$PWD"
cmake --build build/product-composition -j1
build/product-composition/native_product_map_composition
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
The finite exact CPU example explicitly admits 512 million logical rational
and sparse slots for the retained proof histories. Library defaults stay fixed.
These logical bounds do not measure physical memory or allocator use. The maps
declare rational chain realizations; structural type and occurrence transport
requires its own declared morphism.
