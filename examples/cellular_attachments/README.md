# Cellular attachments

Exact transports between distinct vertex stalks, ordered face charts and declared
higher incidence maps over Q and Q[a]/(a^3 - 2). The example retains a triangle,
an inactive fourth vertex, two face charts and boundaries through grade four.
Additional cases expose full curvature, a flat partial chart that hides loop
curvature, a defect only at grade four, and a weighted reversed repeated loop.

Build from a checkout:

```sh
cmake -S rexgraphcpp/examples/cellular_attachments -B build/cellular-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/cellular-example -j1
build/cellular-example/cellular_attachments
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
Output is one JSON record per case, including complete rational carrier matrices,
every source occurrence, boundary, adjacent defect, face prefix and path action.

`PreparedCellularAttachment::declare` requires actual carrier owners, invertible
edge connections and tail charts, and a chart for each declared attaching loop.
Other occurrences, including inactive legs and higher attachments, need explicit
maps. Choose `RequireFlat` or `DeclaredDeformation`; homology rejects nonflat
realizations. A face chart can be rectangular, so its boundary defect and the
full loop defect are reported separately.

Owned paths retain unreduced steps. Face-move receipts record the source move,
the resulting paths, full transport equality and triviality of every used loop
as separate facts. A structural face move need not preserve a curved transport.
