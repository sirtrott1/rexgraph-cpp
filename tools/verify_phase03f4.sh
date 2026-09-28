#!/usr/bin/env bash
# Out-of-source F4 matrix. No writes to source or frozen references.
# RFC_VERIFY_CLANG=1 enables atlas, accession, source safety and E observation suites.
# RFC_F3_EVIDENCE=/path/to/F3_evidence.zip enables frozen-byte comparison.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase03f4.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Verification directory must be outside source' >&2; exit 2;; esac
suites=('atlas:rfc_native_atlas_tests' 'native:rfc_native_accession_tests' 'declaration_temporal:rfc_declaration_temporal_tests'
 'declarations:rfc_boolean_declaration_tests' 'safety:rfc_source_safety_tests'
 'observation:rfc_relational_code_observation_tests' 'code_delta:rfc_relational_code_delta_tests'
 'code:rfc_relational_code_tests' 'segment:rfc_temporal_segment_tests' 'finite:rfc_temporal_native_tests')
for pair in "${suites[@]}"; do
 name=${pair%%:*}
 [[ ! -e "$WORK/gcc_${name}_fixtures" ]] || { echo 'Use a fresh verification directory' >&2; exit 2; }
done
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-1}"
ctest --test-dir "$WORK/build" --output-on-failure
for pair in "${suites[@]}"; do
 name=${pair%%:*};target=${pair#*:}
 "$WORK/build/$target" "$WORK/gcc_${name}_fixtures" | tee "$WORK/gcc_${name}.log"
done
python3 "$SOURCE/tools/verify_native_atlas_reference.py" "$WORK/gcc_atlas_fixtures" --report "$WORK/independent_atlas.json"
python3 "$SOURCE/tools/verify_native_accession_reference.py" "$WORK/gcc_native_fixtures" --report "$WORK/independent_accession.json"
python3 "$SOURCE/tools/verify_temporal_declarations_reference.py" "$WORK/gcc_declaration_temporal_fixtures" --report "$WORK/independent_temporal_reference.json"
python3 "$SOURCE/tools/verify_boolean_layout_reference.py" "$WORK/gcc_declarations_fixtures" --report "$WORK/independent_reference.json"
for pair in 'atlas:rfc_native_atlas_demo' 'accession:rfc_native_accession_demo' 'declarations:rfc_boolean_declaration_demo' \
 'declaration_temporal:rfc_declaration_temporal_demo' 'observation:rfc_rfcx_relational_code_observation_demo' \
 'delta:rfc_rfcx_relational_code_delta_demo' 'code:rfc_rfcx_relational_code_demo' 'temporal:rfc_rfcx_temporal_demo'; do
 name=${pair%%:*};target=${pair#*:}
 "$WORK/build/$target" "$WORK/demo_$name" | tee "$WORK/demo_$name.log"
done
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 for pair in 'atlas:test_native_atlas' 'native:test_native_accession' \
             'safety:test_source_safety' 'observation:test_relational_code_observations'; do
  name=${pair%%:*};file=${pair#*:}
  for mode in clang sanitize; do
   flags=(-O2)
   if [[ $mode == sanitize ]]; then flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer); fi
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
    -I"$SOURCE/include" "$SOURCE/tests/storage/$file.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_${name}"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$WORK/${mode}_${name}" "$WORK/${mode}_${name}_fixtures" | tee "$WORK/${mode}_${name}.log"
  done
 done
 python3 - "$WORK" <<'PY'
import hashlib,json,pathlib,sys
root=pathlib.Path(sys.argv[1]);records=[]
for suite in ('atlas','native','safety','observation'):
 for p in sorted((root/f'gcc_{suite}_fixtures').glob('*')):
  if not p.is_file() or p.suffix not in ('.rfcxa','.rfcxam','.rfca','.rfcx','.rfcxl','.rfcxe','.rfcxt','.rfco','.rfcxd','.rfcxv','.rfcxds','.rfcxdu','.rfcxsm'): continue
  if p.stem in ('rejected','truncated','mutation'): continue
  data=p.read_bytes()
  for mode in ('clang','sanitize'):
   q=root/f'{mode}_{suite}_fixtures'/p.name
   if not q.is_file() or q.read_bytes()!=data: raise SystemExit('Byte mismatch: '+str(q))
  records.append(dict(suite=suite,file=p.name,sha256=hashlib.sha256(data).hexdigest()))
(root/'cross_build_bytes.json').write_text(json.dumps(dict(status='passed',count=len(records),records=records),indent=2)+'\n')
print(f'{len(records)} binary fixture files identical across GCC, Clang and sanitizers.')
PY
fi
if [[ -n ${RFC_F3_EVIDENCE:-} ]]; then
 python3 - "$WORK" "$RFC_F3_EVIDENCE" <<'PY'
import hashlib,json,pathlib,sys,zipfile
root=pathlib.Path(sys.argv[1]);records=[]
with zipfile.ZipFile(sys.argv[2]) as z:
 names=z.namelist()
 for suite in ('native','declaration_temporal','declarations','safety','observation','code_delta','code'):
  for p in sorted((root/f'gcc_{suite}_fixtures').glob('*')):
   if not p.is_file() or p.suffix not in ('.rfca','.rfcx','.rfcxl','.rfcxe','.rfcxt','.rfco','.rfcxd','.rfcxv','.rfcxds','.rfcxdu','.rfcxsm'): continue
   if p.stem in ('rejected','truncated','mutation'): continue
   matches=[n for n in names if n.endswith(f'/gcc_{suite}_fixtures/{p.name}')]
   if len(matches)!=1 or z.read(matches[0])!=p.read_bytes(): raise SystemExit('Missing or changed F3 fixture: '+str(p))
   records.append(dict(suite=suite,file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
(root/'f3_byte_compatibility.json').write_text(json.dumps(dict(status='passed',count=len(records),records=records),indent=2)+'\n')
print(f'{len(records)} binary fixture files unchanged from the supplied F3 evidence.')
PY
fi
printf 'Verification outputs: %s\n' "$WORK"
