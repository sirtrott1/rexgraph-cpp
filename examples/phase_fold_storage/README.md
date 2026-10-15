# Phase and fold storage

This example saves and restores exact phase operations, panel folds, and cyclic
path lifts. It prints four winding cases and 49 fold cases, including complete
finite interactions after editing the restored word.

```sh
cmake -S rexgraphcpp/examples/phase_fold_storage -B build/phase-fold-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp" -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase-fold-example -j1
build/phase-fold-example/phase_fold_storage
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to its
installation prefix. Link `rfc::storage` and include
`rfc/storage/phase_fold_codec.hpp`. The Linux durable adapter is a separate include,
`rfc/storage/phase_fold_durable.hpp`.

```cpp
auto archive = rfc::storage::PhaseFoldArchive::declare(history);
auto draft = archive.propose(7, 0, replacement_phase, true);
rfc::storage::write_phase_fold_archive(draft, "fold.rfcxw");
auto restored = rfc::storage::read_phase_fold_archive("fold.rfcxw");
auto changed = restored.apply(7);
auto observed = restored.observed_prefix(1);
```

RFCXW v1 retains the complete RFCX source, initial phase or panel assembly, original
ordered operations, supplied inactive values, and a separate prospective edit
table. A cyclic walk retains its polygon order, branch, initial integer lift,
generator, and unreduced edge traversals. Winding comes from the closed lifted
path. It is not reconstructed from a scalar phase.

Each read creates fresh carrier and coefficient owners. Applying a proposal uses
the restored carrier and replays later operations, including moving world crease
frames. It clears the prior proposal table; the original archive remains usable.
An observed prefix excludes all proposals and later operations. Complete finite
attribution is available between the original restored word and its edited branch.
Independent reads require separately declared comparisons for cross-owner analysis.

`PhaseFoldDurableHistory::create(path, archive, channel, source_namespace)` stores
the archive in an explicitly named native octet channel. `append(transaction,
expected_root, revisions)` publishes an ordered group. `at(sequence)` selects an
exact publication revision; `observed_prefix(sequence, count)` selects its operation
prefix. `status(transaction)` resolves publication after reopening. Identical
retries require the same predecessor and complete frame bytes.

The durable adapter uses the existing exclusive Linux coordinator and flush/recovery
contract. Publication revisions, operation positions, prospective edits, and physical
time remain separate. Snapshot replacement does not declare a carrier transport or
a continuous physical fold. File publication alone does not provide this durable
transaction contract.

`PhaseFoldLimits` bounds the wire image, source, rational coordinates, coefficient
integers, steps, panel/cover declarations, and prospective table. History limits
also bound replayed endpoints and finite attribution. Policies are reader/service
limits and are not part of the mathematical declarations or canonical bytes.

Use `--export directory` to write the five example `.rfcxw` files into a new
directory. The program uses create-new publication and preserves existing files.
