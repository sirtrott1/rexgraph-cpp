# Database and tensors on a graph revision

Select an exact native graph publication, query its actual archive owners, and
contract fields through the original relational boundary.

```sh
cmake -S rexgraphcpp/examples/graph_database -B build/graph-database-demo \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp" -DCMAKE_BUILD_TYPE=Release
cmake --build build/graph-database-demo -j1
build/graph-database-demo/graph_database build/new-graph-database
```

The store directory must be new. Installed consumers omit `RFC_SOURCE_DIR`, set
`CMAKE_PREFIX_PATH` and link `rfc::native_query_execution`.

```cpp
rfc::runtime::WorkAvailability available{revision.source_namespace(), revision.sequence()};
auto database = rfc::database::AlgebraicGraphDatabase::select(revision, available);
auto archive = database.archive(archive_key, available);
auto query = rfc::database::PreparedAlgebraicQuery::prepare(archive, request, available);
auto field = rfc::database::AlgebraicGradeTensor::select(archive, grade, vector_keys, available);
auto lower = field.boundary(available);
```

Archive selections and result pins retain the native endpoint, archive key,
actual bundle and complete graph source associations. Shared graph owners remain
shared. Independent equal sources and separate revision restores remain independent.
Queries and their same operation cache use the existing complete field tuples.

A grade tensor requires one explicitly named vector per original cell, in source
cell order. Each vector must belong to that actual stalk. The retained stalk
offsets provide reversible coordinates for heterogeneous ranks. Boundary action
uses the original prepared occurrence maps with signing, activity and magnitude
already realized once. It preserves repeated, inactive and supported zero source
occurrences and retains the complete input with each derived lower grade field.

`AlgebraicGradeTensorWindow` gives that full stalk direct sum an explicitly chosen
reversible shape and box. Every visible coordinate resolves to its original cell
and local stalk coordinate. Shape does not introduce new cells, a comparison or
a metric. Supported zeros remain coordinates; an empty box has no coefficient
sum. The optional sum is a declared coefficient contraction, separate from a
carrier vector, trace or metric quadrance.

The example writes three graph revisions over Q(i), Q(zeta_8) and Q(sqrt(2)),
checks declared cross field action on the queried carriers, shows cached queries
and native boundary tensors, and keeps cutoff 41 after revision 42 is published.
Use `--export new-directory` for the complete `.rfcxh` sources. Publication
ordinals remain separate from event/availability clocks. Snapshot replacement
supplies no implicit cross revision transport. Execution limits and cancellation
use the existing scoped exact CPU policy.
