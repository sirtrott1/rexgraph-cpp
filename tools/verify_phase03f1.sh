#!/usr/bin/env bash
# Out-of-source F1 matrix; does not edit the source or reference fixtures.
# Optional: RFC_VERIFY_CLANG=1 for three focused Clang/sanitizer suites.
# Optional: RFC_F0_EVIDENCE=/path/to/F0_evidence.zip for frozen byte comparison.
set -euo pipefail
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase03f1.XXXXXX")}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Verification directory must be outside source' >&2; exit 2;; esac
for pair in 'declarations:rfc_boolean_declaration_tests' 'safety:rfc_source_safety_tests' \
            'observation:rfc_relational_code_observation_tests' 'code_delta:rfc_relational_code_delta_tests' \
            'code:rfc_relational_code_tests'; do
    name=${pair%%:*}
    if [[ -e "$WORK/gcc_${name}_fixtures" ]]; then
        echo "Use a fresh fixture directory: $WORK/gcc_${name}_fixtures" >&2; exit 2
    fi
done
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-2}"
ctest --test-dir "$WORK/build" --output-on-failure
for pair in 'declarations:rfc_boolean_declaration_tests' 'safety:rfc_source_safety_tests' \
            'observation:rfc_relational_code_observation_tests' 'code_delta:rfc_relational_code_delta_tests' \
            'code:rfc_relational_code_tests' 'segment:rfc_temporal_segment_tests' \
            'native:rfc_temporal_native_tests'; do
    name=${pair%%:*}; target=${pair#*:}
    "$WORK/build/$target" "$WORK/gcc_${name}_fixtures" | tee "$WORK/gcc_${name}.log"
done
python3 "$SOURCE/tools/verify_boolean_layout_reference.py" "$WORK/gcc_declarations_fixtures" \
    --report "$WORK/independent_reference.json"
"$WORK/build/rfc_boolean_declaration_demo" "$WORK/declaration_demo"
for pair in 'observation:rfc_rfcx_relational_code_observation_demo' \
            'delta:rfc_rfcx_relational_code_delta_demo' 'code:rfc_rfcx_relational_code_demo' \
            'temporal:rfc_rfcx_temporal_demo'; do
    name=${pair%%:*}; target=${pair#*:}
    "$WORK/build/$target" "$WORK/demo_$name"
done
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
    CLANG=${CLANGXX:-clang++}
    for pair in 'declarations:test_declarations' 'safety:test_source_safety' \
                'observation:test_relational_code_observations'; do
        name=${pair%%:*}; file=${pair#*:}
        for mode in clang sanitize; do
            flags=(-O2)
            if [[ $mode == sanitize ]]; then
                flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
            fi
            "$CLANG" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
                -I"$SOURCE/include" -DRFC_TEST_SOURCE_DIR="\"$SOURCE\"" \
                "$SOURCE/tests/storage/$file.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_${name}"
            ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
                "$WORK/${mode}_${name}" "$WORK/${mode}_${name}_fixtures" | tee "$WORK/${mode}_${name}.log"
        done
    done
    python3 - "$WORK" <<'PY'
import hashlib, json, pathlib, sys
root=pathlib.Path(sys.argv[1]); records=[]
for suite in ('declarations','safety','observation'):
    for p in sorted((root/f'gcc_{suite}_fixtures').glob('*')):
        if not p.is_file() or p.suffix not in ('.rfcx','.rfcxl','.rfcxe','.rfcxt','.rfco','.rfcxd','.rfcxv'): continue
        if p.stem in ('rejected','truncated','mutation'): continue
        data=p.read_bytes()
        for mode in ('clang','sanitize'):
            q=root/f'{mode}_{suite}_fixtures'/p.name
            if q.read_bytes()!=data: raise SystemExit('Byte mismatch: '+str(q))
        records.append(dict(suite=suite,file=p.name,sha256=hashlib.sha256(data).hexdigest()))
(root/'cross_build_bytes.json').write_text(json.dumps(dict(status='passed',count=len(records),records=records),indent=2)+'\n')
print(f'{len(records)} fixture files byte-identical across GCC, Clang and sanitizers (including fixed rejection fixtures).')
PY
fi
if [[ -n ${RFC_F0_EVIDENCE:-} ]]; then
    python3 - "$WORK" "$RFC_F0_EVIDENCE" <<'PY'
import hashlib,json,pathlib,sys,zipfile
root=pathlib.Path(sys.argv[1]); records=[]
with zipfile.ZipFile(sys.argv[2]) as z:
    names=z.namelist()
    for suite in ('safety','observation','code_delta','code','segment','native'):
        for p in sorted((root/f'gcc_{suite}_fixtures').glob('*')):
            if not p.is_file() or p.suffix not in ('.rfcx','.rfcxl','.rfcxe','.rfcxt','.rfco'): continue
            if p.stem in ('rejected','truncated','mutation'): continue
            hits=[n for n in names if n.endswith(f'/final_gcc_{suite}_fixtures/{p.name}')]
            if len(hits)!=1 or z.read(hits[0])!=p.read_bytes(): raise SystemExit('F0 byte mismatch or missing fixture: '+str(p))
            records.append(dict(suite=suite,file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
(root/'f0_byte_compatibility.json').write_text(json.dumps(dict(status='passed',count=len(records),records=records),indent=2)+'\n')
print(f'{len(records)} fixture files unchanged from supplied F0 archive.')
PY
fi
printf 'Verification outputs: %s\n' "$WORK"
