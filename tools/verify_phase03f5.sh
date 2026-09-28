#!/usr/bin/env bash
# Out-of-source exact F5 verification. SQLite is optional; no network/provider calls.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase03f5.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Verification must be outside source' >&2; exit 2;; esac
[[ ! -e "$WORK/gcc_interpretation_fixtures" ]] || { echo 'Use a fresh verification directory' >&2; exit 2; }
SQLITE=OFF
[[ ${RFC_VERIFY_SQLITE:-0} == 1 ]] && SQLITE=ON
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-1}"
ctest --test-dir "$WORK/build" --output-on-failure
"$WORK/build/rfc_native_interpretation_tests" "$WORK/gcc_interpretation_fixtures" | tee "$WORK/gcc_interpretation.log"
"$WORK/build/rfc_native_interpretation_demo" "$WORK/demo_interpretation" | tee "$WORK/demo_interpretation.log"
if [[ $SQLITE == ON ]]; then
 "$WORK/build/rfc_sqlite_interpretation_tests" "$WORK/gcc_sqlite_fixtures" | tee "$WORK/gcc_sqlite.log"
 python3 "$SOURCE/tools/verify_interpretation_reference.py" "$WORK/build/rfc_sqlite_interpretation_tests" --report "$WORK/independent_sqlite.json"
fi
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 suites=('interpretation:database/test_native_interpretation')
 [[ $SQLITE == ON ]] && suites+=('sqlite:io/test_sqlite_interpretation')
 for pair in "${suites[@]}"; do
  name=${pair%%:*};file=${pair#*:};libs=(-lgmpxx -lgmp)
  [[ $name == sqlite ]] && libs+=(-lsqlite3)
  for mode in clang sanitize; do
   flags=(-O2)
   [[ $mode == sanitize ]] && flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
      -I"$SOURCE/include" "$SOURCE/tests/$file.cpp" "${libs[@]}" -o "$WORK/${mode}_${name}"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
      "$WORK/${mode}_${name}" "$WORK/${mode}_${name}_fixtures" | tee "$WORK/${mode}_${name}.log"
  done
 done
 python3 - "$WORK" <<'PY'
import hashlib,json,pathlib,sys
root=pathlib.Path(sys.argv[1]);records=[]
for group in ('interpretation','sqlite'):
 for p in sorted((root/f'gcc_{group}_fixtures').glob('*')):
  if p.suffix not in ('.rfcxi','.rfcxiu'): continue
  data=p.read_bytes()
  for mode in ('clang','sanitize'):
   assert (root/f'{mode}_{group}_fixtures'/p.name).read_bytes()==data, str(p)
  records.append(dict(suite=group,file=p.name,sha256=hashlib.sha256(data).hexdigest()))
(root/'cross_build_bytes.json').write_text(json.dumps(dict(status='passed',count=len(records),records=records),indent=2)+'\n')
print(len(records),'canonical interpretation fixtures identical across builds')
PY
fi
python3 "$SOURCE/tools/verify_interpretation_archive_reference.py" "$WORK/gcc_interpretation_fixtures" --report "$WORK/independent_archive.json"
printf 'Verification outputs: %s\n' "$WORK"
