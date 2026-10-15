# Repeated native homotopy composition

`PreparedNativeHomotopySequence::prepare` owns a declared native homotopy and an
explicit finite stage bound. `append` returns a new sequence with a supplied
certified domain cylinder and complete pre-, interval and post maps. The earlier
sequence remains usable. No homotopy is solved or inferred from numerical values.

Each stage retains its original map declarations and actual native publications.
The cumulative certificate checks every cell and original occurrence against the
direct root composition, including inactive and zero flags and unused product
type interpretations. Explicit identity maps preserve the full homotopy map;
identity operations still remain distinct entries in the ordered history.

Availability includes the root's four publication roles and four more per stage.
Each cutoff is admitted separately, including roles sharing an actual owner.
Every retained publication contributes its integer and work limits. Actions use
only the current domain's actual chain or product owner; output boundaries retain
their sequence and predecessor. Appending publishes no source or temporal history.

Build from the workspace root:

```sh
cmake -S rexgraphcpp/examples/native_homotopy_sequence \
  -B build/native-homotopy-sequence-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/native-homotopy-sequence-example -j1
build/native-homotopy-sequence-example/native_homotopy_sequence
```

For installed use, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to the
installation prefix. The example prepares three complete compositions and checks
the exact signed prism identity. This is a bounded rational reference profile;
its container envelopes do not measure RSS, allocator capacity or GMP internals.
