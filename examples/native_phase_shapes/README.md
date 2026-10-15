# Exact phases and shapes

The example prints a rational phase, a finite cyclic order, circle/disk/sphere
homology, and generic versus exceptional transported fields of a trefoil
presentation. All calculations are exact. The shape constructions specify
topology; no circumference or metric embedding is inferred.

Build from a checkout:

```sh
cmake -S rexgraphcpp/examples/native_phase_shapes -B build/native-phase-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/native-phase-example -j1
build/native-phase-example/native_phase_shapes
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to
its installation prefix. The attaching profile requires a one vertex pairwise
presentation through grade two with a declared common invertible transport.
The trefoil calculation is a field observation, not a complete knot classifier.
