# Native relative pairs

A filled face and its complete retained boundary define a relative pair inside
one native ambient source. The quotient has one face coordinate and no boundary
coordinates. The source keeps repeated, inactive, zero and cancelling occurrences,
including an inactive edge route and a declared empty grade.

```sh
cmake -S examples/native_relative -B build/native_relative -G Ninja \
  -DRFC_SOURCE_DIR="$PWD"
cmake --build build/native_relative --parallel 1
build/native_relative/native_relative
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.

`PreparedNativeRelativePair` owns the actual ambient publication and explicit
whole/subcomplex selections. Both selections must be closed on every retained
boundary occurrence. Preparation uses the canonical relative constructor and
checks the ambient's complete strict chain family. Quotient coordinates name
whole cells outside the subcomplex; they do not become new primary relations.

`project` maps a whole chain to the quotient. `inject` includes a subcomplex chain
into the whole complex. `representative` chooses zero values on the subcomplex;
it is a declared coordinate section, not a chain inverse. The three coordinate
spaces have independent local positions with complete maps back to the ambient.
Boundaries retain encountered zero coordinates and filter only the explicitly
quotiented subcomplex directions.

`connecting_witness` requires a relative cycle. It retains that cycle, the chosen
representative, its full whole-complex boundary and the boundary in the subcomplex.
Zero coordinates outside the subcomplex stay inspectable in the whole boundary.
The witness does not compute homology groups or certify a topological embedding.

`PreparedNativeRelativePairMap` owns both actual pairs and a complete strict
ambient cell/occurrence map. It must preserve the whole and subcomplex selections.
The induced maps retain original directions and supported cancellation; no inverse,
metric adjoint or schema recoding is inferred.

Every native action checks original/current namespace and sequence cutoffs and
current exact CPU resource limits. Foreign or later equal-valued fields reject.
Outputs retain their owners and input history. Counts describe named containers
and visits, not total process memory or performance. GPU kernels, tensor-stalk
quotients and durable quotient publication require separate contracts.
