# Native database preview

`rfcdb` is a standalone C++20 consumer of the library. It stores the same owning
native history in a packed file or immutable directory. It requires a POSIX host,
GMP/GMPXX, OpenSSL Crypto, CMake 3.24 or later, and Python 3 for tests. OpenSSL
belongs to this envelope and does not enter the mathematical core.

From the library repository root:

```sh
cmake -S examples/database_preview -B build/database -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/database --parallel 1
ctest --test-dir build/database --output-on-failure
./build/database/rfcdb sample build/sample.db 9
./build/database/rfcdb inspect build/sample.db
./build/database/rfcdb field build/sample.db 5 10 10
./build/database/rfcdb pack build/sample.db build/sample.rfcxdb
./build/database/rfcdb unpack build/sample.rfcxdb build/copy.db
./build/database/rfcdb pack build/copy.db build/copy.rfcxdb
cmp build/sample.rfcxdb build/copy.rfcxdb
```

Use fresh output paths. The field reading `211/243` is a synthetic discrete model
value. Native source identities, repeated and inactive occurrences, tensor
declarations, and complete history survive the round trip.

`import-series` and `export-series` exchange canonical RFCXS histories. `append`
validates an update stream and writes a new directory. `branch` inspects a
retained relation's occurrences; `placement` returns component offsets and sizes.
The native input boundary here is RFCXS or the C++ API.

The separate `sample-action` profile retains scalar cell stalks and upper fields
in a native history. `execute` applies the compiled owning field contribution and prints
the selected declarations, occurrence bindings, resource bounds, decision
provenance, and complete exact lower support. Coordinates `0` require explicit
maps; a nonzero argument invokes the named constant fiber constructor and checks
the declared common coordinate identity.

```sh
./build/database/rfcdb sample-action build/action.db 1
./build/database/rfcdb execute build/action.db 1 1 9000001
./build/database/rfcdb pack build/action.db build/action.rfcxdb
./build/database/rfcdb execute build/action.rfcxdb 1 1 9000001
```

The final optional argument is `cpu`, `required-gpu`, or `allow-cpu-fallback`.
Only the exact CPU reference is available. Results remain unpublished, and
execution leaves the source history and packed publication unchanged.

Directories create `COMMIT` after their components; opening validates the manifest
and native history. SHA-256 checks integrity. Packed images and reconstructed
histories are held in memory, and cold opening validates the complete history.
The preview has no WAL, recovery protocol, or concurrent writer service.

For independent fixture checks:

```sh
./build/database/database_tests build/database-fixtures
python3 examples/database_preview/tools/reference_check.py build/database-fixtures \
  --report build/database-reference.json
```

`tools/verify.sh` supplies the GCC and optional Clang/sanitizer workflow.
Set `RFC_SOURCE_DIR` to override the default library checkout.
