#!/usr/bin/env bash
# Run outside the source tree; serial C++ compilation bounds peak compiler memory.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:-$(mktemp -d "${TMPDIR:-/tmp}/rfc-phase04.XXXXXX")}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
case "$WORK/" in "$SOURCE/"*) echo 'Use a work directory outside source' >&2; exit 2;; esac
[[ ! -e "$WORK/gcc_defect_fixtures" ]] || { echo 'Use a fresh verification directory' >&2; exit 2; }
SQLITE=OFF; [[ ${RFC_VERIFY_SQLITE:-1} == 1 ]] && SQLITE=ON
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --parallel "${RFC_PARALLEL:-1}"
ctest --test-dir "$WORK/build" --output-on-failure | tee "$WORK/ctest.log"
"$WORK/build/rfc_native_defect_tests" "$WORK/gcc_defect_fixtures" | tee "$WORK/gcc_defect.log"
python3 "$SOURCE/tools/verify_defect_reference.py" "$WORK/gcc_defect_fixtures" --report "$WORK/independent_defects.json"
for pair in 'execution:rfc_binary_execution_tests' 'declarations:rfc_boolean_declaration_tests' \
 'code:rfc_relational_code_tests' 'interpretation:rfc_native_interpretation_tests' \
 'atlas:rfc_native_atlas_tests' 'native:rfc_native_accession_tests' \
 'declaration_temporal:rfc_declaration_temporal_tests' 'safety:rfc_source_safety_tests' \
 'observation:rfc_relational_code_observation_tests' 'code_delta:rfc_relational_code_delta_tests' \
 'segment:rfc_temporal_segment_tests' 'finite:rfc_temporal_native_tests'; do
 name=${pair%%:*}; target=${pair#*:}
 "$WORK/build/$target" "$WORK/gcc_${name}_fixtures" | tee "$WORK/gcc_${name}.log"
done
python3 "$SOURCE/tests/cli/test_local.py" "$WORK/build/rfc_local" --work "$WORK/gcc_cli" --report "$WORK/gcc_cli.json"
if [[ $SQLITE == ON ]]; then
 "$WORK/build/rfc_sqlite_interpretation_tests" "$WORK/gcc_sqlite_fixtures" | tee "$WORK/gcc_sqlite.log"
 python3 "$SOURCE/tools/verify_interpretation_reference.py" "$WORK/build/rfc_sqlite_interpretation_tests" --report "$WORK/independent_sqlite.json"
fi
if [[ ${RFC_VERIFY_CLANG:-0} == 1 ]]; then
 for mode in clang sanitize; do
  flags=(-O2); [[ $mode == sanitize ]] && flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  "${CLANGXX:-clang++}" -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
   -I"$SOURCE/include" "$SOURCE/tests/storage/test_native_defects.cpp" -lgmpxx -lgmp -o "$WORK/${mode}_defects"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
   "$WORK/${mode}_defects" "$WORK/${mode}_defect_fixtures" | tee "$WORK/${mode}_defect.log"
 done
 python3 "$SOURCE/tools/verify_phase04_evidence.py" "$WORK" --cross-build --report "$WORK/cross_build.json"
fi
if [[ -n ${RFC_F6_EVIDENCE:-} ]]; then
 [[ $SQLITE == ON ]] || { echo 'Full frozen F6 evidence includes SQLite fixtures: enable RFC_VERIFY_SQLITE' >&2; exit 2; }
 python3 "$SOURCE/tools/verify_phase04_evidence.py" "$WORK" --f6-evidence "$RFC_F6_EVIDENCE" --report "$WORK/frozen_f6.json"
fi
"$WORK/build/rfc_native_defect_demo" "$WORK/disk_demo" | tee "$WORK/disk_demo.log"
# Timings run after compilation, and include full native equality checks.
"$WORK/build/rfc_native_defect_profile" 3 > "$WORK/route_profile.csv"
printf 'Verification outputs: %s\n' "$WORK"
