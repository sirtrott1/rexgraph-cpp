# Native metric relative novelty

This example runs the paper's rectangular comparison and path change cause
pairings on stored native Fields and a declared positive Metric Tensor. Both
endpoints, carriers and their complete comparison are explicit.

```sh
cmake -S rexgraphcpp/examples/native_metric_novelty -B build/metric-novelty \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/metric-novelty -j1
build/metric-novelty/native_metric_novelty
```

An installed consumer uses `-DCMAKE_PREFIX_PATH=/path/to/install` in place of
`RFC_SOURCE_DIR`.

`PreparedNativeMetricNovelty::prepare(request, cutoffs)` checks the entire new
metric for symmetry and positivity. Its prepared Gram solve rejects a comparison
that loses rank. `execute(old_field, new_field, cutoffs)` returns the whole
innovation, the persistent coordinate change, its realization, the metric
complement and their exact pairings. The rectangular example gives quadrances
`425/13` and `108/13`, zero cross pairing and total `41`.

`execute_causes` also takes an ordered list of complete stored Fields on the
actual new carrier. It checks that their sum equals the innovation and returns
the full Gram tensor. Repetitions, zeros, cancellation and all cross terms remain
present. The path change example gives `[[16,4],[4,2]]` and total `26`; adding the
diagonal alone would give `18`. Supplied channels are declared attributions; their
operation origins must be established separately.

The complement is relative to this comparison and metric. It does not establish
physical birth. The left inverse is not asserted to be the adjoint for an
unsupplied old metric. No dense inverse or projector is assembled. Results own
their actual publications and inputs, and never substitute later or foreign
equal key data. Preparation and current endpoint cutoffs are both required.

This bounded rational CPU profile defaults to a dimension limit of 64, covering
both carrier sizes and the complete channel axes. Current resource and
cancellation checks include retained factors, complete inputs and a conservative
envelope for named scalar containers, even when the innovation is zero. These
counts do not measure total allocation or GMP memory. Calls use separate scratch;
they neither publish native data nor cache host process results.
