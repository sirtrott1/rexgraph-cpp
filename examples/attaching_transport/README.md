# Ordered attachment transport

This example assigns two different invertible rational maps to retained
generators. They satisfy the trefoil attachment and violate the commutator
attachment. It prints the complete curvature and flat homology dimensions.

```sh
cmake -S rexgraphcpp/examples/attaching_transport -B build/attachment-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/attachment-example -j1
build/attachment-example/attaching_transport
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.
The named constructor declares common rational coordinates for every cell and
requires one map per primary generator. Steps follow source occurrence order;
the next map multiplies on the left on column vectors. Inverses are exact.
The supported source is a complete one-vertex pairwise presentation through
grade two. Curved realizations retain their source and reject homology.
