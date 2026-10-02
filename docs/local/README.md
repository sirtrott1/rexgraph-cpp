# Local CLI

`rfc_local` reads native snapshots, performs exact queries, and writes new
snapshots and ordered histories. SQLite import is optional. Enable
`RFC_ENABLE_SQLITE_INTERPRETATION` when configuring the build to use it.

## Snapshot example

Run from the repository root after building:

```sh
mkdir -p build/trial
./build/release/rfc_local sample build/trial/base.rfcxi
./build/release/rfc_local inspect build/trial/base.rfcxi
./build/release/rfc_local show build/trial/base.rfcxi 10
./build/release/rfc_local sum build/trial/base.rfcxi 10 1
./build/release/rfc_local sample build/trial/changed.rfcxi changed
./build/release/rfc_local diff build/trial/base.rfcxi build/trial/changed.rfcxi build/trial/change.rfcxiu
./build/release/rfc_local replay build/trial/base.rfcxi build/trial/change.rfcxiu build/trial/replayed.rfcxi
cmp build/trial/changed.rfcxi build/trial/replayed.rfcxi
```

The first exact sum is `3`; the changed sum is `3/2`. Duplicate assertions remain
distinct. An unknown quantity and a supported zero have different meanings.
Output paths must be new; rerunning this example requires a fresh directory.

## SQLite import

```sh
python3 tools/make_local_example.py build/trial/example.sqlite
./build/release/rfc_local import-sqlite build/trial/example.sqlite build/trial/imported.rfcxi \
  --namespace 900 --revision snapshot/1 \
  --subject 1 people id --assertion 10 events id \
  --reference 10 11 1 sender --reference 10 12 1 recipient \
  --assertion 20 people id
./build/release/rfc_local inspect build/trial/imported.rfcxi
```

Bindings declare identity and participation roles. External references resolve
at ingestion; equal values do not establish identity. INTEGER, TEXT, and BLOB
are supported under the adapter's admitted schema. REAL values, generic SQL,
implicit rowid, and automatic schema migration are outside this profile.
`refresh-sqlite` retains the identity ledger and rejects changed declarations.

## Ordered histories

```sh
./build/release/rfc_native_series_demo build/trial/series
./build/release/rfc_local series-create build/trial/series/base.rfcx build/trial/series/base.rfcxe build/trial/base.rfcxs 700
./build/release/rfc_local series-append build/trial/base.rfcxs build/trial/series/history.rfcxt build/trial/history.rfcxs
./build/release/rfc_local series-inspect build/trial/history.rfcxs
./build/release/rfc_local series-at build/trial/history.rfcxs 42
```

Sequences identify retained endpoints. Checkpoints accelerate replay after full
archive validation. Repacking preserves every ordered operation, including
operations with no current numerical effect.

## Contracts

Successful commands emit JSON; errors return a nonzero status. Numeric values and
64-bit identities are JSON strings. The display is a derived observation; native
archives supply canonical replay. Every invocation loads and validates its input.
The CLI uses exclusive output creation. Crash recovery, multiwriter transactions,
and an authorization service are outside the current local profile.

Use `rfc_local --help` for the complete command list and `rfc_local doctor` for
compiled feature availability.
