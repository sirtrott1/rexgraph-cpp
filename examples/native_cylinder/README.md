# Native cylinders

The cylinder of a weighted branch keeps one original branching relation at each
endpoint and one prism relation. Its four boundary participants produce four
side occurrences; the canonical interval supplies two endpoint occurrences.

```sh
cmake -S examples/native_cylinder -B build/native_cylinder -G Ninja \
  -DRFC_SOURCE_DIR="$PWD"
cmake --build build/native_cylinder --parallel 1
build/native_cylinder/native_cylinder
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.

`PreparedNativeCylinder` takes two authentic empty Chain anchors, distinct
factor identities and a complete external type product declaration. The second
factor must retain the entire canonical `RelationalInterval` declaration.
The owned product stays lazy: its cells are ordered identity pairs, without
Cartesian allocation, pairwise branch expansion or scalar key reification.

`chain` accepts a declared source chain or a native Chain from the actual source
publication and execution authority. `source_boundary` retains inactive and
zero routes. `endpoint` includes the chain at Start or Finish. `prism` sweeps it
without a sign; `homotopy` returns the signed prism `H_k(x)=(-1)^k(x x I)`.
`boundary` uses the canonical graded product action. Supported zero is distinct
from absence throughout these operations.

`homotopy_witness` retains the input, both endpoints, source boundary, signed
prism, `dH`, `Hd`, endpoint difference and full supported residual. It checks
`dH + Hd = finish - start` exactly. At grade zero the absent lower source grade
is recorded separately from the explicitly zero `Hd` field. This chain identity
does not supply a declared relational morphism from the cylinder to another
complex or certify a geometric homotopy.

`PreparedNativeCylinderMap` owns two cylinders and complete strict source and
interval maps, including every unused cell and occurrence. It retains the
external type crosswalk derived from the two declarations. The induced chain
map fixes the declared interval endpoints and commutes with boundaries and
signed prisms. Noninjective source maps retain supported cancellation. Its
mapped result owns both cylinders, complete maps and the original input.

Every action checks original/current source and interval cutoffs. Maps require
both cutoffs independently for both cylinders. Foreign or later equal valued
fields reject. Exact rational CPU limits apply to full sources and unused maps,
including empty actions. Counts describe named containers and visits rather
than total allocator/GMP memory or performance. Tensor stalk cylinder maps,
geometric homotopy morphisms, coverings, GPU and durable services have separate
contracts.
