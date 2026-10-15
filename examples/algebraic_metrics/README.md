# Exact algebraic metrics

This consumer declares compatible selected real or complex embeddings and field
involutions, then certifies positive module metrics with native exact LDL factors.
It applies and solves forms, constructs algebraic duals and metric adjoints, and
runs boundary, coboundary and Hodge actions on a complete grade-four cellular
source. Every original stalk and occurrence remains retained.

```sh
cmake -S rexgraphcpp/examples/algebraic_metrics -B build/algebraic-metrics-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp" -DCMAKE_BUILD_TYPE=Release
cmake --build build/algebraic-metrics-example -j1
build/algebraic-metrics-example/algebraic_metrics
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
The executable writes exact rational JSON records for a real cubic field and
Hermitian fields of degrees two, four and eight.

Use `AlgebraicMeasurementForm` for a declared Hermitian form, including a singular
pullback. `AlgebraicModuleMetric::certify` checks its exact positive pivots at the
selected embedding. `solve` reuses those factors on the complete native carrier.
Local forms assemble through `pullback` and `add`; certification rejects any
remaining unmeasured direction. `AlgebraicDualMap` acts on linear covectors without
a metric. `algebraic_metric_adjoint` supplies the separately declared adjoint.

A positive metric at one embedding does not imply a positive metric on the full
rational scalar restriction. `certify_rational_trace_metric` requests and checks
that additional condition explicitly. No embedding projection, floating
approximation, wire format or continuous physical model is inferred.
