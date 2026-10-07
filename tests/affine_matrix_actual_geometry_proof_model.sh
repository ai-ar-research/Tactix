#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$2
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}
trestle=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
model=$root/tools/fuzz/checkmodel
for tool in "$trim" "$trestle" "$model"; do
  [[ -x "$tool" ]] || exit 77
done
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
"$binary" "$temporary/geometry"
"$trim" -f -c "$temporary/geometry.cnf" "$temporary/geometry.dsr" "$temporary/geometry.lsr" > "$temporary/trim.log" 2>&1
grep -q '^s VALID$' "$temporary/trim.log"
"$trestle" "$temporary/geometry.cnf" "$temporary/geometry.lsr" c > "$temporary/trestle.log" 2>&1
grep -q '^s VALID$' "$temporary/trestle.log"
"$model" "$temporary/geometry.cnf" "$temporary/geometry.model"
# The planted model makes x1 false: this unit is not an unconditional consequence.
printf '1 0\n' > "$temporary/bad.dsr"
if "$trim" -f -c -U "$temporary/geometry.cnf" "$temporary/bad.dsr" "$temporary/bad.lsr" > "$temporary/bad.log" 2>&1; then
  echo 'Unsupported unconditional geometry unit was accepted'
  exit 1
fi
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/bad.log"
echo 'affine_matrix_actual_geometry_proof_model OK'
