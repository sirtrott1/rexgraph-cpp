# Batch attachment through retained native Hodge frames

Companion Theorem 15.1 fixes `B_k` and the selected metric `M_k`, then appends
closed columns `Z`. If `r = rank(Pi_h Z)` and there are `m` new cells, the batch
removes `r` harmonic dimensions in grade `k` and adds `m-r` homology dimensions
in grade `k+1`.

This consumer implements the source mutation and the field consequences:

```cpp
auto batch = PreparedNativeBatchHarmonicAttachment::prepare(
    retained_native_frames, {actual_attachment_declarations, dimension_limit},
    availability, execution_lease);
auto result = batch.execute(actual_stored_field_pin, availability, execution_lease);
```

`core/PreparedBatchAttachment` copies the original source once, appends the
declared batch, and checks every existing cell, occurrence identity, role
aspect, exact lattice coefficient and scale against the retained prefix. It
preserves explicit empty grades. New incidences must name actual existing lower
cells. The declarations retain zero-aggregate and repeated occurrences.

`field/PreparedBatchHarmonicAttachment` uses the old complete Hodge frame owner,
projects sparse candidate columns through the cached harmonic action, and
prepares only an independent residual frame and its metric Gram solve. For an
input field, that residual projection moves from harmonic to curl; gradient is
unchanged. All numerical fields and actions in this path stay sparse.

The complete kernel of the residual matrix supplies `m-r` new upper-cycle
witnesses. Each is lifted through the cached old curl coordinates to actual old
upper-cell positions. The resulting coordinates in `[old upper cells; new
cells]` are checked against the full appended boundary. This retains the
difference between a new harmonic direction that is filled and a dependent
attachment that creates upper homology.

The native wrapper owns the actual old publication, metric pins, field pin,
source, Hodge frames, batch declaration and resulting staged source. The output
is a derived receipt for the staged mutation. It does not forge a publication
pin for a source that has not been published. `staged_source()` exposes the
complete owned declaration for the application's actual publication flow.
`sparse_values()` exposes the updated sectors and transferred component.

The fixture stages three cells on an unfilled square: one face, a proportional
face, and a zero-aggregate relation with two retained opposite occurrences.
Exactly one harmonic direction moves to curl and two upper-cycle witnesses are
created. The selected metric is a declared coupled rational positive form.
The returned result survives destruction of the accession directory because
all of its native dependencies are retained.

The numerical tests compare every selected-carrier basis response against an
independently rebuilt Hodge declaration, including mixed metrics, old-boundary
lifts and grade-zero component merging. A whole new upper-grade metric family
is not inferred by this selected-grade update; the subsequent publication
declares any such new metrics explicitly.

Build from the repository root with
`cmake -S examples/batch_harmonic_attachment -B build/batch_harmonic_attachment -DRFC_SOURCE_DIR="$PWD"`,
then `cmake --build build/batch_harmonic_attachment` and
`build/batch_harmonic_attachment/batch_harmonic_attachment`.
An installed package can be used by omitting `RFC_SOURCE_DIR` and supplying its
`CMAKE_PREFIX_PATH`. The main build also exposes
`rfc_batch_harmonic_attachment_demo` when `RFC_BUILD_APPS=ON`.
