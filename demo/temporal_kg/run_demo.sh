#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${ROOT}/build-temporal-kg-demo"
cmake -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DRFC_BUILD_APPS=ON \
  -DRFC_BUILD_BENCHMARKS=OFF \
  -DRFC_BUILD_CAPABILITY_DEMO=OFF >/dev/null
cmake --build "${BUILD}" --target rfc_temporal_kg_showcase --parallel 1 >/dev/null
exec "${BUILD}/rfc_temporal_kg_showcase" --data-dir "${ROOT}/demo/temporal_kg/data" "$@"
