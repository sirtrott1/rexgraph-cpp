# Algebraic database queries

Build against the source tree or an installed RFC package:

```sh
cmake -S . -B build -DRFC_SOURCE_DIR=/path/to/rexgraphcpp
cmake --build build -j1
build/algebraic_database /path/to/real_cubic.rfcxa
```

For an installed package, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.
The consumer prints every named record, all coordinate windows, exact equality
queries and retained occurrence maps. Vector, covector, map, measurement form and
positive metric records retain their original types, field and carriers.
Matrix axes are target then source; each entry retains the complete power-basis
coefficient tuple. Every declared entry is supported, including zero. An empty
window has no sum. A window sum contracts coefficients; it is not a metric reading.

`PreparedAlgebraicQuery` accepts a record kind, actual source/target carriers and
ordered equality or inequality predicates on complete algebraic entries. A
predicate coordinate must exist in every candidate record. Results own record
pins. `AlgebraicQueryCache` reuses the same immutable prepared operation; a fresh
restore or separately prepared operation requires evaluation. Its limit counts
result pins, not total archive bytes or process memory.

`AlgebraicHistoryWindow::select` requires an explicit inclusive sequence range,
namespace cutoff and archive channel. Sequences select retained snapshots; they
are not event times. Independently restored snapshots have distinct owners. An
explicit field/carrier comparison is required to transport values between them.

Generate and run the C++ notebook example:

```sh
python3 notebook.py /path/to/algebraic-data.rfnb
rfcnb run /path/to/algebraic-data.rfnb /path/to/algebraic-data-state
rfcnb run /path/to/algebraic-data.rfnb /path/to/algebraic-data-state
```

The first cell creates `measures.rfcxa`; the second queries it and writes
`query.json` and an identical `measures-copy.rfcxa`. Notebook cells cache the
native binary artifacts independently. JSON is a derived observation. The current
notebook Database page uses RFCXI; these algebraic files are consumed through
native C++ cells. They are not recoded into the rational Database page schema.
