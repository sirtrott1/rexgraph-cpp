# Native selected field observations

This example solves a declared rational operator on a native source-grade field
and reads requested coordinates through a complete native Accession tensor.
The output carrier, original coordinates, repeated requests and stable source
cell identities remain explicit.

```sh
cmake -S rexgraphcpp/examples/native_observations -B build/native-observations-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/native-observations-example -j1
build/native-observations-example/native_observations
```

For an installed library, use `-DCMAKE_PREFIX_PATH=/path/to/install` instead of
`RFC_SOURCE_DIR`.

`PreparedNativeWoodburySolve::prepare_base_ordered` accepts complete row and
column ordering words. `PreparedNativeSelectedObservation::prepare` adopts that
actual factor owner and verifies the entire declared selector map. On the
32-vertex star, minimum-degree ordering reduces retained elimination entries
from 496 to 31. These are factor-container counts; they do not measure execution
time, hardware cache use or process memory. The source retains all 155 original
occurrences.

The selected star response is `(1/33, 2/33, 1/33, 1/33)`, with the last source
coordinate requested twice. The square example reads relation coordinates
`(2, 0, 2)` before and after a face attachment. Its results are
`(2/15, 7/15, 2/15)` and `(-1/15, 4/15, -1/15)`. The old plan still reads its
original endpoint after the directory advances.

Preparation and execution require namespace/sequence availability witnesses.
Every use rechecks current work, integer growth and retained-factor limits,
including an empty output request. Complete coordinate coverage uses the primal
solve; a proper subset reuses exact transpose-solve certificates. Neither path
forms a complete inverse. Results own their native plan and input field. They
are derived observations and do not publish a new source, claim reconstruction
or infer a carrier transport.
