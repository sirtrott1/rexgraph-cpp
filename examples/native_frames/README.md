# Native exact Hodge frames

Build from the library repository root:

```sh
cmake -S examples/native_frames -B ../build/native-frames-example -G Ninja -DRFC_SOURCE_DIR="$PWD"
cmake --build ../build/native-frames-example -j1
../build/native-frames-example/native_frames
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.

The example decomposes a square edge field before and after face attachment,
retaining the original repeated, cancelling and inactive occurrences. The
harmonic direction becomes a curl direction after attachment. It also prepares
grade two frames in a grade three complex with a declared nonidentity metric.

`PreparedNativeHodgeFrames::prepare(publication, request, availability, lease)`
retains an actual native endpoint and execution authority. The request selects
one grade, every actual SourceGrade carrier in grade order, and either the
named unit metric or one complete declared positive metric per grade. Empty
grades are valid. All grade metrics and all adjacent boundary products are
checked before preparing the selected grade's sectors.

`execute(field, availability, lease)` requires a complete stored Field on the
actual selected carrier and endpoint. It returns full gradient, curl and
harmonic fields, their separate frame coordinates and their exact reconstructed
sum. The result owns its input and plan. Frame coordinates refer to that plan's
derived basis axes; equal dimensions do not create native identity or transport.

The canonical `field::PreparedHodgeFrames` obtains gradient directions by
solving the current metric on an independent image frame of the boundary's
algebraic transpose. Positive lower metrics are invertible, so this has exactly
the image of the metric adjoint. Curl uses the complete upper boundary image;
harmonic directions use the kernel of the stacked boundary and metric dual
upper constraint. Each sector retains a Gram solve. No eigenvectors, dense
inverse, complete adjoint matrix, Hodge matrix or projector is required.

`numeric::PreparedExactFrames` prepares image and kernel frames together and
retains the complete factorization `A = image * coordinates`. Pivot and free
columns refer to the original coordinate order. Supported native zeros remain
distinct from absent support even when numerical reduction cancels entries.

Availability requires an explicit namespace and maximum sequence at preparation
and execution. Preparation cutoffs remain binding; later same key Fields reject.
Current record, scalar, bit, work and native accession caps apply to each action,
including zero fields and empty carriers. The dimension limit defaults to 64
and covers whole maps, including stacked harmonic constraints. Counts describe
scoped selected/retained containers and full outputs, not total GMP/RSS or timing.
The service prepares factors and creates no result cache or source publication.
