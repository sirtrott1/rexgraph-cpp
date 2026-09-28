# Temporal knowledge graph example

The CSV files declare entities, facts, contacts, and queries for the standalone
`rfc_temporal_kg_showcase` executable. Run from the repository root:

```sh
cmake --build build/release --target rfc_temporal_kg_showcase --parallel 1
./build/release/rfc_temporal_kg_showcase --data-dir demo/temporal_kg/data
```

The example compares declared temporal and field observations over retained
source data. Its application conventions do not define new library semantics.
