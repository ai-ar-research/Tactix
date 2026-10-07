#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$2
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}
trestle=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
drat=${DRAT_TRIM:-$root/tools/checkers/bin/drat-trim}
model=$root/tools/fuzz/checkmodel
for checker in "$trim" "$trestle" "$drat" "$model"; do
  [[ -x "$checker" ]] || exit 77
done
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
"$binary" "$temporary/normalized"
shopt -s nullglob
inputs=("$temporary"/normalized*.cnf)
[[ ${#inputs[@]} -gt 0 ]]
for input in "${inputs[@]}"; do
  stem=${input%.cnf}
  "$model" "$input" "$stem.model"
  if [[ -f "$stem.dsr" ]]; then
    # Native matrix reasons may introduce fresh XOR definitions by checked RAT.
    "$trim" -f -c "$input" "$stem.dsr" "$stem.lsr" > "$stem.trim.log" 2>&1
    grep -q '^s VALID$' "$stem.trim.log"
    "$trestle" "$input" "$stem.lsr" c > "$stem.trestle.log" 2>&1
    grep -q '^s VALID$' "$stem.trestle.log"
  fi
  if [[ -f "$stem.drat" ]]; then
    status=0
    "$drat" "$input" "$stem.drat" -f > "$stem.drat.log" 2>&1 || status=$?
    [[ $status -eq 1 ]]
    tr -d '\r' < "$stem.drat.log" | grep -q '^s DERIVATION$'
    grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$stem.drat.log"
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
  if "$trim" -f -c -U "$input" "$stem.unsupported" "$stem.bad.lsr" > "$stem.bad.log" 2>&1; then
    echo 'Unsupported unconditional normalization unit was accepted'
    exit 1
  fi
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$stem.bad.log"
done
echo 'affine_root_normalization_proof_model OK'
