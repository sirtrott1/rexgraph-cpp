# Native linear observations

The example returns `S solve(A, b)` from stored rational operators, accession
maps and fields. The source is a square's relational complex. Its boundary
defines `A = I + B^T B`; attaching a face adds the exact term `C C^T`.

The accession has five declared rows: two different linear readings, a repeated
reading, a supported zero, and a proportional reading. All five outputs remain
in order. The repeated coefficient row shares one transpose certificate;
the proportional row keeps its own certificate. No inverse or reduced carrier
is constructed. An identity accession demonstrates the full primal solve path.

From the repository root:

```sh
cmake -S examples/native_linear_observations -B build/native-linear-observations \
  -DRFC_SOURCE_DIR="$PWD" -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-linear-observations -j1
build/native-linear-observations/native_linear_observations
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`
pointing to the installation prefix.

`PreparedNativeLinearObservation::prepare` adopts an actual native base solve
and the complete Accession map. Preparation and execution require namespace and
sequence cutoffs. Each stored input must have complete support on the actual
SourceGrade carrier. Results retain the map, field and factor owners; an old
plan continues to read its old endpoint after the face is attached.

When there are fewer than `n` distinct nonzero coefficient rows, the plan checks
each retained certificate `A^T z = s^T` before returning `z^T b`. Otherwise it
uses the original primal solve and applies the complete `S`. This row count
policy avoids preparing all inverse rows; it does not assert a performance
advantage for every map. Current limits cover complete input, retained factors,
coefficient and index counts, named vector slots, exact integer growth and work.
These counts are scoped requirements, not process memory measurements.
