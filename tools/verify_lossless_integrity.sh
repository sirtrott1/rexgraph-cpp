#!/usr/bin/env bash
# All generated state stays outside the source. Compiler jobs are sequential;
# several header-heavy translation units exceed modest hosts' parallel budgets.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-lossless.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Use an external verification directory' >&2; exit 2;; esac
[[ ! -e "$WORK/io-gcc" ]] || { echo 'Use a fresh output directory' >&2; exit 2; }
SQLITE=OFF; [[ ${RFC_VERIFY_SQLITE:-0} == 1 ]] && SQLITE=ON
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --parallel 1
ctest --test-dir "$WORK/build" --output-on-failure -j1 | tee "$WORK/ctest.log"
"$WORK/build/rfc_lossless_io_tests" "$WORK/io-gcc" | tee "$WORK/io-gcc.log"
"$WORK/build/rfc_native_model_integrity_tests" "$WORK/model-gcc" | tee "$WORK/model-gcc.log"
python3 "$SOURCE/tools/verify_lossless_reference.py" "$WORK/model-gcc" "$WORK/io-gcc" --report "$WORK/reference-gcc.json"
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 for mode in clang sanitize; do
  flags=(-O2); [[ $mode == sanitize ]] && flags=(-O1 -g0 -fsanitize=address,undefined -fno-omit-frame-pointer)
  for suite in model io; do
   file=test_native_model_integrity.cpp; [[ $suite == io ]] && file=test_lossless_io.cpp
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Werror \
      -I"$SOURCE/include" "$SOURCE/tests/audit/$file" -lgmpxx -lgmp -o "$WORK/$mode-$suite"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
      "$WORK/$mode-$suite" "$WORK/$suite-$mode" | tee "$WORK/$suite-$mode.log"
  done
  python3 "$SOURCE/tools/verify_lossless_reference.py" "$WORK/model-$mode" "$WORK/io-$mode" --report "$WORK/reference-$mode.json"
 done
 python3 - "$WORK" <<'PY'
from pathlib import Path
import json,sys
w=Path(sys.argv[1]); files=[]
for p in sorted((w/'io-gcc').iterdir()):
 if p.is_symlink() or not p.is_file() or p.name=='race.bin': continue
 for mode in ('clang','sanitize'): assert p.read_bytes()==(w/f'io-{mode}'/p.name).read_bytes(),p.name
 files.append(p.name)
for mode in ('clang','sanitize'):
 assert (w/'model-gcc/multiplication.csv').read_bytes()==(w/f'model-{mode}/multiplication.csv').read_bytes()
(w/'cross-build.json').write_text(json.dumps({'status':'passed','files':files+['multiplication.csv'],'exclusions':'race winner and filesystem objects are nondeterministic'},indent=2)+'\n')
PY
fi
printf 'Verified outputs: %s\n' "$WORK"
