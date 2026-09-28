#!/usr/bin/env bash
# Reproduce in an out-of-source work tree. Set RFC_VERIFY_CLANG=1 for the
# four-suite optimized-Clang and ASan/UBSan matrix used for this checkpoint.
set -euo pipefail
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase03d.XXXXXX")}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-2}"
ctest --test-dir "$WORK/build" --output-on-failure
for pair in 'code_delta:rfc_relational_code_delta_tests' 'code:rfc_relational_code_tests' \
            'segment:rfc_temporal_segment_tests' 'native:rfc_temporal_native_tests'; do
    name=${pair%%:*}; target=${pair#*:}
    "$WORK/build/$target" "$WORK/gcc_${name}_fixtures"
done
"$WORK/build/rfc_rfcx_relational_code_delta_demo" "$WORK/delta_demo"
"$WORK/build/rfc_rfcx_relational_code_demo" "$WORK/code_demo"
"$WORK/build/rfc_rfcx_temporal_demo" "$WORK/temporal_demo"
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
    CLANG=${CLANGXX:-clang++}
    command -v "$CLANG" >/dev/null
    for pair in 'code_delta:test_relational_code_deltas' 'code:test_relational_codes' \
                'segment:test_temporal_segments' 'native:test_temporal_native'; do
        name=${pair%%:*}; file=${pair#*:}
        for mode in clang sanitize; do
            flags=(-O2)
            if [[ $mode == sanitize ]]; then
                flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
            fi
            "$CLANG" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic \
                -I"$SOURCE/include" -DRFC_TEST_SOURCE_DIR="\"$SOURCE\"" \
                "$SOURCE/tests/storage/$file.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_${name}"
            ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
                "$WORK/${mode}_${name}" "$WORK/${mode}_${name}_fixtures"
        done
    done
    python3 - "$WORK" <<'PY'
import hashlib, pathlib, sys
root = pathlib.Path(sys.argv[1])
for suite in ('code_delta', 'code'):
    reference = root / ('gcc_' + suite + '_fixtures')
    for p in sorted(reference.glob('*')):
        if p.is_file() and p.suffix in ('.rfcx', '.rfcxe', '.rfcxt'):
            value = hashlib.sha256(p.read_bytes()).digest()
            for mode in ('clang', 'sanitize'):
                q = root / (mode + '_' + suite + '_fixtures') / p.name
                if hashlib.sha256(q.read_bytes()).digest() != value:
                    raise SystemExit('Cross-build byte mismatch: ' + str(q))
print('Cross-build code fixtures are byte-identical.')
PY
fi
printf 'Verification outputs: %s\n' "$WORK"
