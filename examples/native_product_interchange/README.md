# Owned native product interchange

This example explicitly declares both products and a complete interchange of
all their original type pairs, including unused pairs. It prepares separate
owned AB and BA carriers and applies the exact graded swap
`a ⊠ b ↦ (-1)^(|a||b|) b ⊠ a` to rational fields.

The odd interval pair changes coefficient from `1` to `-1`. The second swap
restores the field, while its native result retains both ordered actions.
The cell witness retains both original boundary words and the permutation
between their factor blocks, including repeated, inactive and zero flags.
The chain witness verifies the round trip and boundary naturality with full
coordinate support. Product closure alone does not declare interchange.

Build from the library source:

```sh
cmake -S examples/native_product_interchange -B build/interchange -DRFC_SOURCE_DIR="$PWD"
cmake --build build/interchange -j1
build/interchange/native_product_interchange
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
Both source cutoffs refer to the original A/B roles, including when the input
field belongs to BA. The finite exact rational CPU profile creates derived
owning observations; it publishes no database endpoint and declares no metric
adjoint, general signature recoding, GPU kernel or tensor-stalk interchange.
