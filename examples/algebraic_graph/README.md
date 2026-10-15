# Algebraic archive graphs

Save and reopen independently declared fields, selected roots, involutions,
metric domains, cell stalk bundles, fiber systems and archives. Named references
preserve shared registered owners within one file. Equal declarations with
different owners remain separate; each file read creates a fresh graph.

```sh
cmake -S rexgraphcpp/examples/algebraic_graph -B build/algebraic-graph-demo \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp" -DCMAKE_BUILD_TYPE=Release
cmake --build build/algebraic-graph-demo -j1
build/algebraic-graph-demo/algebraic_graph build/comparisons.rfcxg
```

The output path must be new. For an installed library, omit `RFC_SOURCE_DIR` and
set `CMAKE_PREFIX_PATH` to its installation prefix.

The example declares Q(i) into Q(ζ₈) by i ↦ ζ₈² and Q(√2) into Q(ζ₈) by
√2 ↦ ζ₈ − ζ₈³. Each carrier comparison retains every target coefficient and
its complete rational action. Proper inclusions have no inverse. Redundant
coordinates use a separately verified reconstruction with R A = I; A R is a
projector on the larger carrier.

Metric domain comparisons check selected roots and involutions. A separate
measurement receipt checks the complete identity C* H_target C = σ(H_source),
including directions absent from a current sample. Root compatibility alone
does not establish an isometry.

RFCXG is a bounded canonical graph container. Its reader rechecks field and root
certificates, all applicable chain laws, measurement identities and reconstruction
pairs. RFCXA archives remain independently readable with their existing format.
The graph does not infer base change, composita, tensor products, temporal clocks
or operation histories. File publication uses the native regular file API;
durable graph history with multiple revisions is a separate contract.
