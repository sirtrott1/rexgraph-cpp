# Declared relational homotopy

This example supplies a complete cylinder carrier and a strict morphism into an
actual native target. `PreparedNativeRelationalHomotopy` checks its declared
product type interpretation, every retained cell and occurrence, both endpoint
restrictions and the complete chain families. Branching relations, inactive
flags, supported zeros and attaching order stay retained.

```sh
cmake -S examples/native_relational_homotopy -B build/homotopy-example \
  -DRFC_SOURCE_DIR="$PWD"
cmake --build build/homotopy-example -j1
build/homotopy-example/native_relational_homotopy
```

An installed consumer uses `-DCMAKE_PREFIX_PATH=/path/to/rfc/install` instead.
The program reports the exact target chain identity
`dK + Kd = second - first`, where `K(x) = H((-1)^grade(x) x ⊗ interval_edge)`.
The result owns the structural certificate and original native Chain input.

Preparation requires an existing `PreparedNativeCylinder`, separate empty Chain
anchors for its supplied carrier and target, a complete
`NativeCylinderCarrierDeclaration`, and three complete `NativeGluingSourceMap`
requests: cylinder to target and both source to target endpoints. Stable factor
pairs name product cells; the upper pair, side and original factor occurrence name
product flags. All four availability witnesses are explicit on each action.

`push` maps an owned cylinder field. `endpoint`, `homotopy`, `boundary` and
`witness` retain their actual input and target owner. Equal keys in unrelated
publications do not bind inputs. Earlier cutoffs reject later carriers. These
actions read pinned sources and do not publish new history.

The supplied carrier must biject the complete product population and preserve
its ordered occurrence fibers. Endpoint types obey the unit law; swept types
declare exact boundaries. The profile uses a common aspect schema and semantic
scale. It does not construct source identities, infer general signature recodings,
solve for homotopies, quotient paths by homotopy, or infer metric adjoints.
