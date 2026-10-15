# Exact algebraic stalks

This consumer selects real algebraic roots, checks declared field embeddings,
applies complete rectangular maps over an algebraic field, and realizes a
relational complex with different stalk ranks. It prints exact JSON records.

```sh
cmake -S examples/algebraic_modules -B build/algebraic-modules \
  -DRFC_SOURCE_DIR="$PWD" -DCMAKE_BUILD_TYPE=Release
cmake --build build/algebraic-modules -j1
build/algebraic-modules/algebraic_modules
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.

`SelectedRealAlgebraicEmbedding::isolate` requires a certified field and a rational
interval containing exactly one real root. The retained Sturm certificate checks
that condition. `sign` and `compare` use exact arithmetic; exhausting a declared
work or bisection bound raises an error. Nonreal root selection beyond the existing
quadratic profile remains outside this API.

`AlgebraicFieldEmbedding::declare` checks a supplied generator image against the
source polynomial. This comparison does not select a spatial root. The separate
`agrees_with` check compares declared real embeddings. Rectangular comparisons
have no inverse.

`AlgebraicStalkBundle::declare` pins the retained complex, coefficient field and
standard free-module basis at every cell. Maps and vectors carry those owners;
equal dimensions or coefficients do not create a carrier identification.
`AlgebraicModuleMap` uses complete row-major field coefficients. Its rational action
places each coefficient's power-basis multiplication matrix in the corresponding
block. Rational rank equals field degree times module rank.

`AlgebraicRelationalFiberSystem` requires an underlying map for every retained
occurrence, including inactive and supported zero occurrences. The native map is
the source's exact signed coefficient times the supplied map. The existing rational
stalk, occurrence and prepared-field code performs the realization. Every adjacent
chain law is checked. `RequireFlat` rejects defects; `DeclaredDeformation` exposes
them and refuses homology when they are nonzero. No metric, timestamp, persistence
format or physical embedding is inferred.
