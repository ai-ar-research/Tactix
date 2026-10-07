#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$2
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}
trestle=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
drat=${DRAT_TRIM:-$root/tools/checkers/bin/drat-trim}
model=$root/tools/fuzz/checkmodel
for tool in "$trim" "$trestle" "$drat" "$model"; do
  [[ -x "$tool" ]] || exit 77
done
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
"$binary" "$temporary/lifecycle"
count=0
for cnf in "$temporary"/lifecycle-*.cnf; do
  base=${cnf%.cnf}
  "$model" "$cnf" "$base.model"
  format=${base##*-}
  case "$format" in
    0|1)
      "$trim" -f -c "$cnf" "$base.proof" "$base.lsr" > "$base.trim.log" 2>&1
      grep -q '^s VALID$' "$base.trim.log"
      "$trestle" "$cnf" "$base.lsr" c > "$base.trestle.log" 2>&1
      grep -q '^s VALID$' "$base.trestle.log"
      ;;
    2|3)
      drat_status=0
      "$drat" "$cnf" "$base.proof" -f > "$base.drat.log" 2>&1 || drat_status=$?
      [[ "$drat_status" -eq 1 ]]
      tr -d '\r' < "$base.drat.log" | grep -q '^s DERIVATION$'
      grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$base.drat.log"
      ;;
    *) exit 1 ;;
  esac
  # No original variable is root-forced. RUP-only checking must reject an
  # unsupported unconditional unit, independent of the positive proof stream.
  printf '%s\n' '-1 0' > "$base.bad.dsr"
  if "$trim" -f -c -U "$cnf" "$base.bad.dsr" "$base.bad.lsr" > "$base.bad.log" 2>&1; then
    echo 'Unsupported unconditional unit was accepted'
    exit 1
  fi
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$base.bad.log"
  count=$((count + 1))
done
[[ "$count" -eq 16 ]]
echo 'affine_matrix_persistent_lifecycle_proof_model OK: 16 original models and proof prefixes'
