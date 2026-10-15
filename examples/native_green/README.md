# Native temporal Green responses

Build from the library repository root:

```sh
cmake -S examples/native_green -B ../build/native-green-example -G Ninja -DRFC_SOURCE_DIR="$PWD"
cmake --build ../build/native-green-example -j1
../build/native-green-example/native_green
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.

The example uses the original square boundary and a retained face birth from
the paper. It displays the three exact response contributions and all four
observed contributions, whose sum is `(-29/15, -29/15)`. Both source complexes
retain their repeated, cancelling and inactive occurrences.

`PreparedNativeGreenResponseDelta::prepare(request, availability, lease)` owns
both native resolving operators and SourceRealization injections. Operators
are complete endomorphisms on actual declared SourceGrade carriers; injections
name their separate linear source coordinate carriers. Both shifted systems
`I + lambda L` must be invertible. No positive metric, injective realization,
inverse matrix or causal model is inferred.

`NativeGreenCorrespondence` declares both actual publication pins, both carrier
keys and a complete supported rational tensor. Supply ambient `J` and source
`K` maps explicitly, including identity maps. Rectangular, zero and rank losing
maps are allowed; partial support and inferred inverses are not. Cross source
comparisons retain both namespaces. Old/new name the requested comparison order.

`execute(old_field, new_field, availability, lease)` accepts complete stored
Fields on the actual source coordinate carriers at their respective endpoints.
Availability supplies separate `WorkAvailability` witnesses for both endpoints
at preparation and execution. Preparation cutoffs remain binding; no request
reads a directory's later publication. Results own both input pins and the plan.

`PreparedNativeObservedGreenResponseDelta::prepare` adopts an actual native
Green plan, both declared Accession operators, and an explicit output comparison
`K_A`. It reuses the same Green factors and prepares the full accession defect.
Its execution returns accession, realization, innovation and operator terms
with the complete direct observed residual. Full maps and unused coordinates
are checked even when the output is empty or `lambda` is zero.

Requests declare a dimension limit, default 64. Every action checks current
host/container/bit/work limits and the selected native accession work caps.
Unsupported exact GPU requests require explicit CPU fallback. Counts describe
scoped selected/retained containers and guard work, not total GMP/RSS or timing.
The result cache and source publication are independent of this derived action;
this service creates neither. General metric novelty decompositions, native
kernel/image frame setup and broader service products remain separate.
