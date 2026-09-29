#!/usr/bin/env bash
# Complete build plus exact series, consumer and independent reference checks.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase05.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Use an external verification directory' >&2; exit 2;; esac
[[ ! -e "$WORK/gcc_series_fixtures" ]] || { echo 'Use a fresh verification directory' >&2; exit 2; }
SQLITE=OFF; [[ ${RFC_VERIFY_SQLITE:-1} == 1 ]] && SQLITE=ON
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-1}"
ctest --test-dir "$WORK/build" --output-on-failure | tee "$WORK/ctest.log"
"$WORK/build/rfc_native_series_tests" "$WORK/gcc_series_fixtures" | tee "$WORK/gcc_series.log"
python3 "$SOURCE/tools/verify_series_reference.py" "$WORK/gcc_series_fixtures" --report "$WORK/independent_series.json"
python3 "$SOURCE/tests/cli/test_series.py" "$WORK/build/rfc_local" "$WORK/build/rfc_native_series_demo" \
  --work "$WORK/gcc_series_cli" --report "$WORK/gcc_series_cli.json"
python3 "$SOURCE/tests/cli/test_local.py" "$WORK/build/rfc_local" --work "$WORK/gcc_cli" --report "$WORK/gcc_cli.json"
cmake -S "$SOURCE/examples/engine_consumer" -B "$WORK/consumer" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRFC_SOURCE_DIR="$SOURCE"
cmake --build "$WORK/consumer" --parallel 1
"$WORK/consumer/engine_math_smoke" | tee "$WORK/consumer.log"
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 for mode in clang sanitize; do
  flags=(-O2); [[ $mode == sanitize ]] && flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
    -I"$SOURCE/include" "$SOURCE/tests/storage/test_native_series.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_series"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$WORK/${mode}_series" "$WORK/${mode}_series_fixtures" | tee "$WORK/${mode}_series.log"
  if [[ ${RFC_VERIFY_CLI:-0} == 1 ]]; then
   extra=(); [[ $SQLITE == ON ]] && extra=(-DRFC_LOCAL_SQLITE=1 -lsqlite3)
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
     -I"$SOURCE/include" "$SOURCE/apps/native_series_demo.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_series_demo"
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
     -I"$SOURCE/include" "$SOURCE/apps/rfc_local.cpp" -lgmpxx -lgmp "${extra[@]}" -o "$WORK/${mode}_local"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
     python3 "$SOURCE/tests/cli/test_series.py" "$WORK/${mode}_local" "$WORK/${mode}_series_demo" \
       --work "$WORK/${mode}_series_cli" --report "$WORK/${mode}_series_cli.json"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
     python3 "$SOURCE/tests/cli/test_local.py" "$WORK/${mode}_local" --work "$WORK/${mode}_cli" --report "$WORK/${mode}_cli.json"
  fi
 done
 python3 "$SOURCE/tools/verify_phase05_evidence.py" "$WORK" --cross-build --report "$WORK/cross_build.json"
fi
"$WORK/build/rfc_native_series_demo" "$WORK/disk_series_demo" | tee "$WORK/disk_series_demo.log"
# Run after compilers finish; verify exact results outside measured intervals.
"$WORK/build/rfc_native_series_profile" 3 > "$WORK/series_profile.csv"
printf 'Verification outputs: %s\n' "$WORK"
