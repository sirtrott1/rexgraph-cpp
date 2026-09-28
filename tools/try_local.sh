#!/usr/bin/env bash
# Build and run only the local preview. Installs nothing and makes no network calls.
set -euo pipefail
SOURCE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${1:?Usage: tools/try_local.sh NEW_WORK_DIRECTORY}
mkdir "$WORK"
WORK=$(cd "$WORK" && pwd)
SQLITE=${RFC_TRIAL_SQLITE:-ON}
cmake -S "$SOURCE" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DRFC_BUILD_TESTS=OFF -DRFC_BUILD_APPS=OFF \
  -DRFC_BUILD_BENCHMARKS=OFF -DRFC_BUILD_LEARNING=OFF -DRFC_BUILD_CAPABILITY_DEMO=OFF \
  -DRFC_BUILD_LOCAL_TOOLS=ON -DRFC_ENABLE_SQLITE_INTERPRETATION="$SQLITE"
cmake --build "$WORK/build" --target rfc_local_tool --parallel "${RFC_PARALLEL:-1}"
CLI="$WORK/build/rfc_local"
"$CLI" doctor
python3 "$SOURCE/tests/cli/test_local.py" "$CLI" --work "$WORK/checks" --report "$WORK/checks.json"
mkdir "$WORK/data"
"$CLI" sample "$WORK/data/base.rfcxi"
"$CLI" show "$WORK/data/base.rfcxi" 10
"$CLI" sum "$WORK/data/base.rfcxi" 10 1
printf 'Local executable: %s\nTrial data: %s\n' "$CLI" "$WORK/data"
