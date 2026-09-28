#!/usr/bin/env bash
set -euo pipefail
paths=(machine numeric composite core algebra local topology field view path statistics temporal database runtime)
fail=0
for module in "${paths[@]}"; do
  while IFS= read -r file; do
    if grep -nE '\b(float|double|long double)\b' "$file" >/tmp/rfc_float_hits 2>/dev/null; then
      echo "floating type in exact/core layer: $file" >&2
      cat /tmp/rfc_float_hits >&2
      fail=1
    fi
  done < <(find "include/rfc/$module" -type f -name '*.hpp' 2>/dev/null | sort)
done
exit "$fail"
