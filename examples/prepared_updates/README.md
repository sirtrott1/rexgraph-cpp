# Prepared exact updates

Build from the repository root:

```sh
cmake -S examples/prepared_updates -B ../build/prepared-updates-example -G Ninja -DRFC_SOURCE_DIR="$PWD"
cmake --build ../build/prepared-updates-example -j1
../build/prepared-updates-example/prepared_updates
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to
the installation prefix.

The example retains a square's repeated and inactive occurrences, then adds its
declared grade-two face. Unit metrics give a rank-one Green update on the
complete grade-one carrier. `PreparedWoodburySolve` reuses an owned base factor
and prepares the complete update directions once. Each `apply` checks the full
updated equation. Its coupling matrix and correction system must be invertible;
`V` represents the explicitly declared algebraic transpose map.

`PreparedGreenResponseDelta` owns both operators, both injections, the ambient
and source correspondences and one rational parameter. Repeated calls return
the separate realization, innovation and operator terms. An owning
`PreparedObservedGreenResponseDelta` additionally prepares the accession defect
and checks the complete observed difference.

These are bounded rational matrix plans. Supply `ExactActionLimits` and an
optional checkpoint to preparation and application, or share an
`ExactActionGuard` across them. Inputs and current limits are checked even for
zero updates, zero parameters and empty observations. Factors are shared;
result vectors and execution scratch are independent. `preparation_steps()`
counts actual guard steps for that preparation, including complete map checks.
No output cache or native endpoint correspondence is inferred. Native field
actions continue to own their original publication and declared carriers.
