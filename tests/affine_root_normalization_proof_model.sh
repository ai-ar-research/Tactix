#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
"$binary" "$temporary/normalized"
shopt -s nullglob
inputs=("$temporary"/normalized*.cnf)
[[ ${#inputs[@]} -gt 0 ]]
for input in "${inputs[@]}"; do
  stem=${input%.cnf}
  "$CHECKMODEL" "$input" "$stem.model"
  if [[ -f "$stem.dsr" ]]; then
    # Native matrix reasons may introduce fresh XOR definitions by checked RAT.
    dsr_valid "$input" "$stem.dsr" "$stem.lsr" "$stem.trim.log" "$stem.trestle.log"
  fi
  # Challenge each original input with a unit opposite its checked model.  The
  # forty-row fixture sets x1 false; the once-only maintenance fixture sets it
  # true.  Neither conditional matrix consequence may justify that opposite
  # unconditional unit.
  if [[ $stem == *-once-* ]]; then
    printf '%s\n' '-1 0' > "$stem.unsupported"
  else
    printf '%s\n' '1 0' > "$stem.unsupported"
  fi
  reject "$stem.bad.log" 'Unsupported unconditional normalization unit was accepted' \
    "$DSR_TRIM" -f -c -U "$input" "$stem.unsupported" "$stem.bad.lsr"
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$stem.bad.log"
done
echo 'affine_root_normalization_proof_model OK'
