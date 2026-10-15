# Native prepared updates

Build from the library repository root:

```sh
cmake -S examples/native_updates -B ../build/native-updates-example -G Ninja -DRFC_SOURCE_DIR="$PWD"
cmake --build ../build/native-updates-example -j1
../build/native-updates-example/native_updates
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to
the installation prefix.

The example declares the paper's exact head change Gram update on one retained
endpoint. The original source keeps its repeated, cancelling and inactive
occurrences. The updated operator is an explicit retained declaration; executing
it produces a derived field without changing or publishing source assertions.

`NativeWoodburyRequest` supplies five complete `Operator` pins: the baseline
`A:X->X`, directions `U:R->X`, coupling `C:R->R`, row map `Rmap:X->R`, and
updated operator `Anew:X->X`. All pins must share the actual endpoint, namespace
and execution authority. `X` is a declared SourceGrade; the update carrier has
its own declared linear coordinates. Numerical zeros require retained support.
Metric and cross form tensor views cannot substitute for operator declarations.

`PreparedNativeWoodburySolve::prepare` checks the entire identity
`Anew = A + U C Rmap` and uses the canonical exact Woodbury solver. `A`, `C`
and the correction must be invertible, including for zero updates. `Rmap` is
declared independently; its numeric transpose supplies the existing solver's
`V` argument without identifying a transpose with a metric adjoint.

Retain `prepare_base(base_pin)` or a plan's `base_solve()` to reuse an actual
native factor across update plans. Adoption requires the same native operator
declaration and publication. Equal coefficients do not establish ownership.
Preparation accepts a declared dimension limit, default 64, through the request.

`execute(field_pin, lease, availability)` requires a complete stored field on
the exact publication and carrier. It rechecks current host limits, the selected
pins' accession work caps, all retained factors and the full updated equation.
The result owns its plan and input and records all operator keys, carrier keys,
namespace, sequence, policies, availability and guard steps. There is no result
cache. Old plans and fields stay pinned after directory advancement; later
fields require their own plan.

Availability is an optional `runtime::WorkAvailability{namespace, max_sequence}`.
Execution preserves the preparation cutoff and also checks the current cutoff.
All factors use bounded rational CPU arithmetic. An unsupported exact GPU
request rejects unless the host policy explicitly permits CPU fallback.
Selected payload and retained scalar/index containers are scoped admission
counts, not total process/GMP memory or timing measurements. Cross endpoint and
rectangular Green comparisons require their own declared correspondence service.
