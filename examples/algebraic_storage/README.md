# Algebraic archives and reconstruction

`AlgebraicArchive` retains one owned coefficient field and stalk bundle, selected
embedding and involution declarations, complete occurrence maps, and named
vectors, covectors, maps, measurement forms and positive metrics. RFCXA version 1
contains canonical rational coefficients and the complete RFCX source. Reopening
rechecks irreducibility, root selection, Hermitian compatibility, every applicable
chain defect and requested metric positivity. Indefinite forms remain forms.

```sh
cmake -S examples/algebraic_storage -B build/algebraic-storage -G Ninja \
  -DRFC_SOURCE_DIR="$PWD" -DCMAKE_BUILD_TYPE=Release
cmake --build build/algebraic-storage -j1
build/algebraic-storage/algebraic_storage build/new-algebraic-archives
```

The optional output directory receives six `.rfcxa` files; existing files are
not replaced. Without the argument, the example prints exact JSON records only.
An installed consumer can replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.

Restored owners are fresh. `AlgebraicCarrierComparison` declares the scalar
embedding and every entry of a carrier map. `AlgebraicReconstruction` checks the
complete equation `R A = I`; its physical projector and `A O R` action retain
redundant directions without declaring a rectangular map invertible. Named
reconstruction declarations retain both referenced map records and recheck
`R A = I` on reopening. The redundant example persists `R* H R` as a singular
measurement form on the physical carrier, positive only on the consistent image.

`AlgebraicArchiveLimits` bounds input bytes, integer magnitudes, record counts,
scalar work and materialized coordinates. These are caller policies, not stored
mathematical data. The integrity checksum is not authentication or owner identity.
`write_algebraic_archive` provides single file publication; it does not promise
power loss durability.

On Linux, `AlgebraicDurableHistory` validates complete typed archives before
publication through the existing `DurableNativeStore`. The
`algebraic_durable_history` example stores two archives as explicitly declared
opaque octet channels, then reopens and
checks both selected sequences:

```sh
build/algebraic-storage/algebraic_durable_history build/new-algebraic-history
```

This uses the store's existing fsync publication, recovery and retry contract.
Archive bytes occupy an explicitly declared sequence of 4 KiB Integer chunks,
with at most 4096 chunks. This supports the default 16 MiB archive limit without
one giant Integer or one value node per octet. Each chunk retains a prefix octet,
and readers validate canonical chunk boundaries and aggregate size before copying.
Archive channels preserve other endpoint data and require the complete endpoint
source. Adding a type to an existing registry requires an explicit declaration
migration. Endpoint selection is by sequence; no event or availability clock is
inferred. RFCX, RFCXE and RFCXT formats retain their existing versions.
