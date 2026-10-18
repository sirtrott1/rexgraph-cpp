# Gluing complete transport families

Two local fits can each satisfy closure and still disagree on their shared
interface. This example declares a filled triangle twice and learns each copy's
three grade two occurrence coefficients with the grade one boundary fixed.
Closure leaves one parameter in each copy.

The interface identifies the common cells and occurrences and supplies exact
accession/reconstruction maps for the original stalks. Comparing the complete
families leaves one common free parameter. Calling `materialize({7/3}, guard)`
selects a value only after compatibility has been established; the family remains
available for other choices.

`fixture.hpp` is a reusable fixture; `main.cpp` checks the resulting occurrence
responses and a forced equality between original member parameters. Include
`rfc/learning/transport_family_gluing.hpp` in consumers. The public constructor is:

```cpp
PreparedTransportFamilyGluing(
    std::vector<PreparedGradedTransportFit> members,
    std::vector<TransportFamilyInterface> interfaces,
    ExactActionGuard& guard);
```

A member may learn any one declared grade, and different members may learn
different grades. Interfaces may be strict retained subcomplexes. Both source
maps must preserve every common cell, occurrence, sign, coefficient, type,
attaching order and identity. Views are indexed by the common cells in every
grade. Reconstruction after accession must be the identity on the original
stalk. Only the intersection of the represented upper input images is shared;
unshared directions remain free.

`particular()` and `kernel()` retain the complete affine family in concatenated
original member parameter coordinates. `coordinate_origin()` and
`coordinate_kernel()` describe the same family in concatenated member free
coordinates. `statuses()` reports whether each interface equation was forced,
excluded or open before the interfaces were imposed jointly. An inconsistent
joint family retains `contradiction()`, a left null vector whose pairing with
`residual_origin()` is nonzero. It cannot be materialized.

Equation assembly stores only active `(output,input)` coordinate keys and
nonzero parameter contributions. Tautological zero equations are omitted.
Every original interface occurrence and its common input/output dimensions
remain in `witnesses()`, including when the whole occurrence comparison is
already forced. A larger declared common carrier therefore does not allocate
an output by input numerical rectangle.

`classify(terms, expected, guard)` classifies a claim against every compatible
member parameter assignment. `materialize(free, guard)` returns the compatible
owned fiber systems, verifies each complete boundary tower, and checks every
realized interface response again. It does not create an undeclared quotient
source or average occurrence weights.

The exact tests also cover mixed selected grades, rectangular stalk views,
unshared input directions, correlated interface inconsistency, owner retention,
bad occurrence maps, cancellation and admission limits. The implementation is
a finite sparse linear constraint construction; it does not claim to solve the
general multigrade polynomial deformation problem.

Build from the repository root with
`cmake -S examples/transport_family_gluing -B build/transport_family_gluing -DRFC_SOURCE_DIR="$PWD" -DRFC_BUILD_LEARNING=ON`,
then `cmake --build build/transport_family_gluing` and
`build/transport_family_gluing/transport_family_gluing`.
An installed package can be used by omitting `RFC_SOURCE_DIR` and supplying its
`CMAKE_PREFIX_PATH`. The main build exposes `rfc_transport_family_gluing_demo`
when both `RFC_BUILD_APPS` and `RFC_BUILD_LEARNING` are enabled. Explicitly
disabling `RFC_BUILD_LEARNING` also disables the standalone example target.
