#!/usr/bin/env bash
set -euo pipefail
files=(
  include/rfc/numeric/exact_matrix.hpp
  include/rfc/field/exact_metric.hpp
  include/rfc/field/exact_hodge.hpp
  include/rfc/field/exact_dirac.hpp
  include/rfc/field/augmented_green.hpp
  include/rfc/learning/exact_metric_channels.hpp
)
fail=0
for file in "${files[@]}"; do
  if grep -nE '\b(float|double|long double)\b' "$file" >/tmp/rfc_exact_metric_float_hits 2>/dev/null; then
    echo "floating type in exact metric path: $file" >&2
    cat /tmp/rfc_exact_metric_float_hits >&2
    fail=1
  fi
done
exit "$fail"
