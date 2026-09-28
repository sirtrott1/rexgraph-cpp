#!/usr/bin/env bash
# Out-of-source repair regression. Add RFC_VERIFY_CLANG=1 for the full focused
# optimized-Clang / ASan+UBSan matrix. Outputs are deliberately outside source.
set -euo pipefail
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase03f0.XXXXXX")}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Verification directory must be outside source' >&2; exit 2;; esac
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-2}"
ctest --test-dir "$WORK/build" --output-on-failure
for pair in 'safety:rfc_source_safety_tests' 'observation:rfc_relational_code_observation_tests' \
            'code_delta:rfc_relational_code_delta_tests' 'code:rfc_relational_code_tests' \
            'segment:rfc_temporal_segment_tests' 'native:rfc_temporal_native_tests'; do
    name=${pair%%:*}; target=${pair#*:}
    "$WORK/build/$target" "$WORK/gcc_${name}_fixtures"
done
for demo in observation delta code temporal; do
    case $demo in
        observation) target=rfc_rfcx_relational_code_observation_demo;;
        delta) target=rfc_rfcx_relational_code_delta_demo;;
        code) target=rfc_rfcx_relational_code_demo;;
        temporal) target=rfc_rfcx_temporal_demo;;
    esac
    "$WORK/build/$target" "$WORK/${demo}_demo"
done
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
    CLANG=${CLANGXX:-clang++}
    for pair in 'safety:test_source_safety' 'observation:test_relational_code_observations' \
                'code_delta:test_relational_code_deltas' 'code:test_relational_codes' \
                'segment:test_temporal_segments' 'native:test_temporal_native'; do
        name=${pair%%:*}; file=${pair#*:}
        for mode in clang sanitize; do
            flags=(-O2)
            if [[ $mode == sanitize ]]; then
                flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
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
root=pathlib.Path(sys.argv[1]); count=0
for suite in ('safety','observation','code_delta','code','segment','native'):
    for p in sorted((root/('gcc_'+suite+'_fixtures')).glob('*')):
        if not p.is_file() or p.suffix not in ('.rfcx','.rfcxl','.rfcxe','.rfcxt','.rfco'): continue
        if p.stem in ('rejected','truncated','mutation'): continue
        expected=hashlib.sha256(p.read_bytes()).digest()
        for mode in ('clang','sanitize'):
            q=root/(mode+'_'+suite+'_fixtures')/p.name
            if hashlib.sha256(q.read_bytes()).digest()!=expected: raise SystemExit('Byte mismatch: '+str(q))
        count+=1
print(f'{count} canonical fixtures are byte-identical across GCC, Clang and sanitizers.')
PY
fi
printf 'Verification outputs: %s\n' "$WORK"
