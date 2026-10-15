# Native covering homotopy

Lift a supplied cylinder homotopy through a strict finite covering from an
explicit initial endpoint. Both incoming and outgoing stars include every
original occurrence. Conflicting shared cells or flags reject during preparation.
The complete lifted morphism and its signed prism chain witness remain owned.

This example lifts the last edge of C3 through C6: the start vertex is 2 and
the finish vertex is 3. It retains six independent native publication roles and
their preparation and execution cutoffs. The covering base comparison is an
explicit map, including when its indexed values form an identity.

```sh
cmake -S rexgraphcpp/examples/native_covering_homotopy -B build/cover-lift \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/cover-lift -j1
build/cover-lift/native_covering_homotopy
```

For an installed library, omit `RFC_SOURCE_DIR` and supply `CMAKE_PREFIX_PATH`.
This profile certifies a supplied finite cylinder; it does not solve homotopies
or identify paths modulo homotopy. Sheet transfer is a separate chain operation.
