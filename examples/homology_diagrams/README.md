# Finite homology diagrams

`rfc/topology/rational_zigzag.hpp` supplies `PreparedRationalZigzag` for finite
ordered rational vector spaces with explicitly oriented consecutive maps. It
retains every map, interval multiplicity and complete generator frame. Each
original arrow is reconstructed exactly through the interval frames.
Interval endpoints are inclusive positions in the supplied finite vertex order.

`rfc/topology/finite_chain_diagram.hpp` supplies
`PreparedRationalChainDiagram`, which checks the complete chain and map identities
and derives the zigzag from the actual cycle and boundary quotients.
`PreparedIntegralModuleDiagram` is a separate product. It retains integral cycle
and boundary lattices, Smith actions, torsion, free coordinates and induced
homomorphisms, with explicit witnesses for preservation of boundaries. It does
not claim a general integral barcode classification.

`rfc/inference/native_homology_diagram.hpp` supplies
`PreparedNativeHomologyDiagram::prepare`. Every vertex names its actual immutable
publication and all of its `SourceGrade` carriers. The request selects rational
zigzag or integral modules and supplies every grade map. Equal dimensions or
equal carrier keys across endpoints do not establish identity. Rational homology
uses the original scaled boundary realization. Integral homology uses the
declared lattice coefficients and requires integral maps. No metric is needed.

`rfc/inference/native_homology_diagram_codec.hpp` retains complete original source
and endpoint channels with the declaration and result certificates. Cold replay
recomputes from source, then checks all certificate bytes. It creates fresh
execution authorities. Rendered numerical results are not mathematical inputs.

The finite profile uses exact CPU actions with the caller's scalar, integer,
container and cancellation policies. Native dimension allowances derive from
the actual source dimensions. Container accounting does not measure process RSS
or the GMP heap. Exact elimination fill and complete generator output retain
their actual storage and arithmetic costs. Missing owners, malformed maps,
failed chain identities, nonintegral integer maps, exhausted policies and altered
codec certificates refuse.

With an installed SDK, build the standalone contracts using:

```sh
cmake -S tests/homology_diagram -B build/homology_diagram -DCMAKE_PREFIX_PATH=/path/to/rfc
cmake --build build/homology_diagram --parallel 1
ctest --test-dir build/homology_diagram --output-on-failure
```

The tests include every arrow orientation on four vertices, independent births,
zero maps, a chain quotient fork, and maps between integral torsion modules.
`zigzag_consumer` accepts a finite textual vector-space declaration on standard
input for independent exact verification. Its input is a case count, then each
case's vertex count, dimensions, and consecutive direction bits (`1` forward,
`0` backward) followed by complete row major rational matrices.
