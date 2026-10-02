#!/usr/bin/env bash
# Build and verify the downstream project. No baseline/paper modifications.
set -euo pipefail
P=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
R=${RFC_SOURCE_DIR:-"$P/../.."}
OUT=${1:?Usage: tools/verify.sh NEW_OUTPUT_DIRECTORY}
if [[ -e "$OUT" || -L "$OUT" ]]; then
    printf 'Output must not exist: %s\n' "$OUT" >&2
    exit 2
fi
mkdir -p -- "$OUT"
OUT=$(cd -- "$OUT" && pwd)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
modes=(gcc)
if [[ ${DB_VERIFY_CLANG:-0} == 1 ]]; then modes+=(clang sanitizer); fi
for mode in "${modes[@]}"; do
    compiler=g++
    flags='-O2 -DNDEBUG'
    extra=()
    if [[ "$mode" == clang || "$mode" == sanitizer ]]; then compiler=clang++; fi
    if [[ "$mode" == sanitizer ]]; then
        flags='-O1 -DNDEBUG'
        extra=(-DDB_SANITIZERS=ON)
    fi
    B="$OUT/$mode"
    cmake -S "$P" -B "$B" -G Ninja -DRFC_SOURCE_DIR="$R" \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$compiler" \
      -DCMAKE_CXX_FLAGS_RELEASE="$flags" "${extra[@]}" >"$OUT/$mode-configure.log" 2>&1
    cmake --build "$B" --parallel 1 >"$OUT/$mode-build.log" 2>&1
    "$B/database_tests" "$B/fixtures" >"$OUT/$mode-tests.log" 2>&1
    python3 "$P/tests/test_cli.py" "$B/rfcdb" >"$OUT/$mode-cli.log" 2>&1
    python3 "$P/tools/reference_check.py" "$B/fixtures" \
      --report "$B/fixtures/reference.json" >"$OUT/$mode-reference.log" 2>&1
    ctest --test-dir "$B" --output-on-failure >"$OUT/$mode-ctest.log" 2>&1
    if [[ "$mode" != gcc ]]; then
        python3 - "$OUT/gcc/fixtures" "$B/fixtures" <<'PY'
import sys
from pathlib import Path
left, right = map(Path, sys.argv[1:])
count = 0
for path in sorted(left.rglob('*')):
    if not path.is_file() or path.is_symlink():
        continue
    relative = path.relative_to(left)
    # Test-only timing files and intentionally incomplete/mutated directories are
    # excluded. These names select canonical product artifacts, not arbitrary
    # files merely sharing an extension.
    if relative.parts[0] in {'sample.db', 'copied.db'} or (
        len(relative.parts) == 1 and
        (path.suffix in {'.rfcxdb', '.rfcxs', '.rfcxt'} or path.name == 'circuit.csv')
    ):
        assert path.read_bytes() == (right/relative).read_bytes(), relative
        count += 1
print(f'canonical_files_compared={count}')
PY
    fi
    printf '%s: passed\n' "$mode"
done
if [[ -n ${RFC_PRODUCT_EXE:-} ]]; then
    python3 "$P/tools/check_phase06_product.py" "$RFC_PRODUCT_EXE" \
      "$OUT/gcc/rfcdb" "$OUT/phase06-product" >"$OUT/phase06-product.log" 2>&1
fi
printf 'Verification complete; logs: %s\n' "$OUT"
