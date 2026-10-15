# Native tensor products

This example declares the external product types for two retained intervals.
Their ordered identity pairs form a square with four vertices, four edges and
one top relation. Twelve original boundary occurrences survive, including
inactive, active zero and cancelling occurrences. The right boundary carries
the odd left grade sign. Its second boundary is exactly zero.

```sh
cmake -S examples/native_tensor_product -B build/native_tensor_product -G Ninja \
  -DRFC_SOURCE_DIR="$PWD"
cmake --build build/native_tensor_product --parallel 1
build/native_tensor_product/native_tensor_product
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.

`PreparedNativeTensorProduct` owns both empty native Chain source anchors,
distinct declared factor identities, the complete external type table and both
namespace/sequence cutoffs. Its `field` method binds explicit supported values
to that actual product. `boundary` retains every encountered occurrence's lower
support, including zero results. Returned fields retain their input and action
receipt. Sources and signature declarations remain unchanged.

`PreparedNativeTensorChainMap` owns complete rational maps at every source and
target factor grade and checks all boundary identities. Its lazy action expands
the nonzero factor map entries while preserving supported zero inputs and
cancellation. Full map shapes retain zero directions. These are linear chain
maps; they do not claim preservation of source types or occurrence fibers.
Targets explicitly declare every source factor grade, using empty grades where
the target space is zero. A rectangular map does not supply an inverse.

The constructor uses native identity pairs rather than allocating all product
coordinates. Explicit cell cardinalities are observations. Factor dimensions,
named container counts, exact integer growth, work and cancellation are bounded
on each call. This binary profile has no implicit scalar key reification,
GPU kernel, persistent product format or general nested product constructor.
