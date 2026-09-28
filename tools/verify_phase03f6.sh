#!/usr/bin/env bash
# Exact portable-execution and local-product verification. Builds serially by
# default: downstream C++20 translation units can require substantial memory.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase03f6.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Verification must be outside source' >&2; exit 2;; esac
[[ ! -e "$WORK/gcc_execution_fixtures" ]] || { echo 'Use a fresh verification directory' >&2; exit 2; }
SQLITE=OFF; [[ ${RFC_VERIFY_SQLITE:-1} == 1 ]] && SQLITE=ON
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRFC_BUILD_LOCAL_TOOLS=ON -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-1}"
ctest --test-dir "$WORK/build" --output-on-failure | tee "$WORK/ctest.log"
for pair in 'execution:rfc_binary_execution_tests' 'declarations:rfc_boolean_declaration_tests' \
 'code:rfc_relational_code_tests' 'interpretation:rfc_native_interpretation_tests' \
 'atlas:rfc_native_atlas_tests' 'native:rfc_native_accession_tests' \
 'declaration_temporal:rfc_declaration_temporal_tests' 'safety:rfc_source_safety_tests' \
 'observation:rfc_relational_code_observation_tests' 'code_delta:rfc_relational_code_delta_tests' \
 'segment:rfc_temporal_segment_tests' 'finite:rfc_temporal_native_tests'; do
 name=${pair%%:*};target=${pair#*:}
 "$WORK/build/$target" "$WORK/gcc_${name}_fixtures" | tee "$WORK/gcc_${name}.log"
done
python3 "$SOURCE/tests/cli/test_local.py" "$WORK/build/rfc_local" --work "$WORK/gcc_cli" --report "$WORK/gcc_cli.json"
python3 "$SOURCE/tools/verify_boolean_layout_reference.py" "$WORK/gcc_declarations_fixtures" --report "$WORK/independent_declarations.json"
if [[ $SQLITE == ON ]]; then
 "$WORK/build/rfc_sqlite_interpretation_tests" "$WORK/gcc_sqlite_fixtures" | tee "$WORK/gcc_sqlite.log"
 python3 "$SOURCE/tools/verify_interpretation_reference.py" "$WORK/build/rfc_sqlite_interpretation_tests" --report "$WORK/independent_sqlite.json"
fi
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 for pair in 'execution:composite/test_execution' 'declarations:storage/test_declarations' 'code:storage/test_relational_codes'; do
  name=${pair%%:*};file=${pair#*:}
  for mode in clang sanitize; do
   flags=(-O2)
   [[ $mode == sanitize ]] && flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
   "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
      -DRFC_TEST_SOURCE_DIR=\""$SOURCE"\" -I"$SOURCE/include" "$SOURCE/tests/$file.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_${name}"
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
      "$WORK/${mode}_${name}" "$WORK/${mode}_${name}_fixtures" | tee "$WORK/${mode}_${name}.log"
  done
 done
 for mode in clang sanitize; do
  flags=(-O2); libs=(-lgmpxx -lgmp)
  [[ $mode == sanitize ]] && flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  [[ $SQLITE == ON ]] && flags+=(-DRFC_LOCAL_SQLITE=1) && libs+=(-lsqlite3)
  "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
      -I"$SOURCE/include" "$SOURCE/apps/rfc_local.cpp" "${libs[@]}" -o "$WORK/${mode}_local"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
      python3 "$SOURCE/tests/cli/test_local.py" "$WORK/${mode}_local" --work "$WORK/${mode}_cli" --report "$WORK/${mode}_cli.json"
 done
 python3 "$SOURCE/tools/verify_f6_evidence.py" "$WORK" --cross-build --report "$WORK/cross_build.json"
fi
if [[ -n ${RFC_F5_EVIDENCE:-} ]]; then
 python3 "$SOURCE/tools/verify_f6_evidence.py" "$WORK" --f5-evidence "$RFC_F5_EVIDENCE" --report "$WORK/frozen_f5.json"
fi
# Run timings only after compilation, not concurrently with the build matrix.
"$WORK/build/rfc_binary_execution_profile" 15 > "$WORK/packed_profile.csv"
"$WORK/build/rfc_native_local_profile" 3 "$WORK/mapped_profile" > "$WORK/native_profile.csv" 2> "$WORK/native_profile_details.log"
if [[ -n ${RFC_BASELINE_INCLUDE:-} ]]; then
 "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -DRFC_EXECUTION_BASELINE -I"$RFC_BASELINE_INCLUDE" \
    "$SOURCE/benchmarks/binary_execution_profile.cpp" -o "$WORK/f5_profile"
 "$WORK/f5_profile" 15 > "$WORK/f5_profile.csv"
fi
cmake --install "$WORK/build" --prefix "$WORK/install" > "$WORK/install.log"
python3 "$SOURCE/tests/cli/test_local.py" "$WORK/install/bin/rfc_local" --work "$WORK/installed_cli" --report "$WORK/installed_cli.json"
printf 'Verification outputs: %s\n' "$WORK"
