# Durable algebraic graph history

Publish exact owner graphs through the native Linux store, reopen their grouped
receipts, and retain earlier revisions while later data are published.

```sh
cmake -S rexgraphcpp/examples/graph_history -B build/graph-history-demo \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp" -DCMAKE_BUILD_TYPE=Release
cmake --build build/graph-history-demo -j1
build/graph-history-demo/graph_history build/new-graph-store
```

The store directory must be new. For an installed library, omit `RFC_SOURCE_DIR`
and set `CMAKE_PREFIX_PATH` to its installation prefix. Link `rfc::storage`.

```cpp
auto base = rfc::storage::AlgebraicGraphSnapshot::anchor_only(graph, anchor_bundle);
auto mapped = base.with_source_morphism(other_bundle, complete_cell_maps);
auto history = rfc::storage::AlgebraicGraphDurableHistory::create(
    path, base, channel, source_namespace);
auto receipt = history.append(transaction, history.root(), revisions);
auto selected = history.prefix(receipt.root.sequence);
auto revision = selected.at(sequence);
auto archive = revision.archive_pin(archive_key);
```

An explicit registered bundle anchors the native source. `anchor_only` leaves all
other sources independent, including equal sources. A source map declares every
cell and retains the complete strict occurrence map, including repeated, inactive,
and supported zero occurrences. Each archive binding names its actual registered
bundle. Source morphisms preserve the primary relational structure; scalar
embeddings, module comparisons, metric receipts, and reconstruction remain the
separate declarations inside the owner graph.

RFCXH v1 wraps unchanged RFCXG v1 bytes with these bindings. Its reader rechecks
all graph certificates, complete source maps, anchor identity, and canonical bytes.
Each separate read creates fresh owners. Shared references inside one revision
remain shared; a revision pin or archive pin keeps them alive after closing the
store. Publishing a replacement graph does not declare transport between revisions.

`at(sequence)` selects one exact publication. `prefix(cutoff)` owns only the base
and frames through that cutoff. Pending revisions, later owner tables, checkpoints,
and tail data are excluded. Sequence numbers denote publication order within the
named source namespace; they are separate from event or availability time.

Grouped publication uses expected roots, immutable objects, exact retry bytes and
the existing exclusive Linux flush/recovery contract. After an unknown outcome,
reopen the store and resolve the transaction with `status(transaction)`. Typed
reopening validates every committed graph before returning a coordinator.

`AlgebraicGraphSnapshotLimits` bounds graph bytes, owners, coefficients, source
bindings, complete map coordinates and retained prefix revisions. The native
octet channel has its existing chunk budget. These are explicit admission policies.
Use `graph_history --export new-directory` to write the two example `.rfcxh` files.
