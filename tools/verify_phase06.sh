#!/usr/bin/env bash
# Integrated retained-source history corpus; all output stays outside source.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase06.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Use an external verification directory' >&2; exit 2;; esac
[[ ! -e "$WORK/gcc_product_fixtures" ]] || { echo 'Use a fresh verification directory' >&2; exit 2; }
SQLITE=OFF; [[ ${RFC_VERIFY_SQLITE:-1} == 1 ]] && SQLITE=ON
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-1}"
ctest --test-dir "$WORK/build" --output-on-failure | tee "$WORK/ctest.log"
"$WORK/build/rfc_temporal_product_tests" "$WORK/gcc_product_fixtures" | tee "$WORK/gcc_product.log"
"$WORK/build/rfc_product_boundary_tests" "$WORK/gcc_boundary_fixtures" | tee "$WORK/gcc_boundary.log"
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 for mode in clang sanitize; do
  flags=(-O2); [[ $mode == sanitize ]] && flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  for item in product boundary; do
   file=test_temporal_product.cpp; [[ $item == boundary ]] && file=test_product_boundary.cpp
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
     -I"$SOURCE/include" "$SOURCE/tests/storage/$file" -lgmpxx -lgmp -o "$WORK/${mode}_${item}"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
     "$WORK/${mode}_${item}" "$WORK/${mode}_${item}_fixtures" | tee "$WORK/${mode}_${item}.log"
  done
 done
 python3 "$SOURCE/tools/verify_phase06_evidence.py" "$WORK" --cross-build --report "$WORK/cross_build.json"
fi
cmake -S "$SOURCE/examples/engine_consumer" -B "$WORK/consumer" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRFC_SOURCE_DIR="$SOURCE"
cmake --build "$WORK/consumer" --parallel 1
"$WORK/consumer/engine_math_smoke" | tee "$WORK/consumer.log"
# Measurements start only once every compiler has finished. These switches choose
# corpus size, not different semantics. An empty/partial benchmark is not success.
runs=()
for size in ${RFC_PRODUCT_SIZES:-8 64 256}; do
 "$WORK/build/rfc_temporal_product" "$WORK/product_${size}" "$size" "${RFC_PRODUCT_TRIALS:-3}" | tee "$WORK/product_${size}.log"
 python3 "$SOURCE/tools/verify_temporal_product_reference.py" "$WORK/product_${size}" --report "$WORK/product_${size}_independent.json"
 runs+=("$WORK/product_${size}")
done
python3 "$SOURCE/tools/summarize_temporal_product.py" "${runs[@]}" --output "$WORK/summary"
"$WORK/build/rfc_product_boundary" "$WORK/boundary_profile" "${RFC_BOUNDARY_ASSERTIONS:-64}" | tee "$WORK/boundary_profile.log"
if [[ $SQLITE == ON ]]; then
 python3 "$SOURCE/tools/verify_interpretation_reference.py" "$WORK/build/rfc_sqlite_interpretation_tests" | tee "$WORK/sqlite_oracle.log"
fi
printf 'Verified outputs: %s\n' "$WORK"
