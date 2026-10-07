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
"$binary" "$temporary"
for stem in record replay; do
  "$trim" -f -c "$temporary/$stem.cnf" "$temporary/$stem.dsr" "$temporary/$stem.lsr" > "$temporary/$stem-trim.log" 2>&1
  grep -q '^s VALID$' "$temporary/$stem-trim.log"
  "$trestle" "$temporary/$stem.cnf" "$temporary/$stem.lsr" c > "$temporary/$stem-trestle.log" 2>&1
  grep -q '^s VALID$' "$temporary/$stem-trestle.log"
  "$model" "$temporary/$stem.cnf" "$temporary/$stem.model"
  printf '1 0\n' > "$temporary/bad.dsr"
  if "$trim" -f -c -U "$temporary/$stem.cnf" "$temporary/bad.dsr" "$temporary/$stem-bad.lsr" > "$temporary/$stem-bad.log" 2>&1; then
    echo 'Unsupported unconditional controller unit was accepted'
    exit 1
  fi
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/$stem-bad.log"
done
cmp "$temporary/record.dsr" "$temporary/replay.dsr"
cmp "$temporary/record.model" "$temporary/replay.model"
echo 'random_controller_affine_resource_proof_model OK'
