#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${source_dir}/build/vulkan-host}"
if (($#)); then shift; fi
mkdir -p -- "$build_dir"
build_dir="$(cd -- "$build_dir" && pwd)"
cmake -S "$source_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@" \
  -DRFC_ENABLE_VULKAN=ON -DRFC_VULKAN_TEST_SOFTWARE=OFF \
  -DRFC_BUILD_TESTS=ON -DBUILD_TESTING=ON \
  -DRFC_BUILD_APPS=OFF -DRFC_BUILD_LOCAL_TOOLS=OFF -DRFC_BUILD_BENCHMARKS=OFF
cmake --build "$build_dir" --target rfc_packed_binary_tests rfc_vulkan_binary_tests --parallel 1
ctest --test-dir "$build_dir" -R 'rfc_(packed_binary_owning|vulkan_binary|vulkan_spirv)' --output-on-failure -j1
cmake -S "$source_dir/examples/vulkan_binary" -B "$build_dir/consumer" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$build_dir/consumer" --parallel 1
"$build_dir/consumer/vulkan_binary" | tee "$build_dir/device-and-calibration.json"
