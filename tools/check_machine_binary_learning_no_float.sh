#!/usr/bin/env bash
set -euo pipefail
files=(
  include/rfc/composite/program.hpp
  include/rfc/learning/binary_gate.hpp
  include/rfc/learning/composite_features.hpp
  include/rfc/machine/bitsliced_int.hpp
  include/rfc/learning/bitsliced_threshold.hpp
  include/rfc/learning/bitsliced_affine.hpp
  include/rfc/learning/integer_perceptron.hpp
  include/rfc/learning/integer_relational_layer.hpp
  include/rfc/learning/operator_channel.hpp
  include/rfc/learning/operator_basis.hpp
  include/rfc/learning/integer_network.hpp
  include/rfc/learning/tower_operator.hpp
  include/rfc/learning/tower_basis.hpp
  include/rfc/learning/tower_network.hpp
  include/rfc/learning/semantic_network.hpp
  include/rfc/field/lattice_field.hpp
  include/rfc/field/lattice_tower.hpp
  include/rfc/field/lattice_boundary.hpp
  include/rfc/numeric/checked_int.hpp
)
fail=0
for file in "${files[@]}"; do
  if grep -nE '\b(float|double|long double)\b' "$file" >/tmp/rfc_binary_float_hits 2>/dev/null; then
    echo "floating type in machine-binary path: $file" >&2
    cat /tmp/rfc_binary_float_hits >&2
    fail=1
  fi
done
exit "$fail"
