#!/usr/bin/env bash
set -euo pipefail
root="${1:-include/rfc}"

declare -A rank=(
  [machine]=0 [numeric]=1 [composite]=2 [core]=3 [algebra]=4 [geometry]=5 [local]=6
  [topology]=7 [field]=8 [view]=9 [path]=9 [approximation]=9 [statistics]=9 [temporal]=10 [storage]=11
  [database]=12 [capability]=12 [runtime]=11 [inference]=13 [learning]=14 [models]=14
  [llm]=15 [multimodal]=15 [agent]=16 [generation]=16 [backend]=17 [io]=17
)

fail=0
while IFS= read -r file; do
  rel="${file#${root}/}"
  module="${rel%%/*}"
  [[ "$module" == "$rel" ]] && continue
  [[ "$module" == "compat" ]] && continue
  current="${rank[$module]:-}"
  [[ -z "$current" ]] && continue
  while IFS= read -r inc; do
    dep="$(sed -E 's@.*"rfc/([^/]+)/.*@\1@' <<<"$inc")"
    [[ "$dep" == "$inc" ]] && continue
    dep_rank="${rank[$dep]:-}"
    [[ -z "$dep_rank" ]] && continue
    if (( dep_rank > current )); then
      echo "layer violation: $file (rank $current) includes $dep (rank $dep_rank): $inc" >&2
      fail=1
    fi
  done < <(grep -E '^#include "rfc/[^/]+/' "$file" || true)
done < <(find "$root" -type f -name '*.hpp' | sort)

exit "$fail"
