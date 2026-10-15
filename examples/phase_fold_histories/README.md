# Exact phase walks and panel folds

This example lifts retained polygon paths through an explicitly declared infinite
cyclic cover. Zero, one and two turns have the same phase endpoint and different
integer winding. The algebraic generator lives in the declared field
Q[x]/(x²−x+1); no spatial embedding or circumference is inferred.

Three attached panels then fold around two declared crease frames using rational
unit phases. The second crease follows its fixed panel. Every shared source vertex
must still glue, and an incomplete moving group rejects. Prospective edits replay
the downstream frames. JSON records include the exact poses, chronological maps,
finite interaction terms and word defect for 49 pairs of rational phases.

```sh
cmake -S examples/phase_fold_histories -B build/phase-fold-example -G Ninja \
  -DRFC_SOURCE_DIR="$PWD" -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase-fold-example --parallel 1
build/phase-fold-example/phase_fold_histories
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH`.
Link `rfc::temporal`. These histories are immutable logical prefixes. They do not
assign event timestamps, write a durable history, or test material contact and
collision. Exact crease kinematics does not establish physical fold feasibility.
