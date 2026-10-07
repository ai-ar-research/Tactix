#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$2
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}
compress=${DSR_COMPRESS:-$root/tools/checkers/bin/dsr-compress}
trestle=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
drat=${DRAT_TRIM:-$root/tools/checkers/bin/drat-trim}
cake=${CAKE_LPR:-$root/tools/checkers/bin/cake_lpr}
model=$root/tools/fuzz/checkmodel
for tool in "$trim" "$compress" "$trestle" "$drat" "$cake" "$model"; do
  [[ -x "$tool" ]] || exit 77
done
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
"$binary" "$temporary"
# Full original-CNF terminal proof plus independently checked every lemma.
python3 "$root/tools/bench/proof_oracle.py" check \
  "$temporary/unsat.cnf" "$temporary/unsat.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$trestle" \
  --drat-trim "$drat" --cake-lpr "$cake"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$temporary/unsat.cnf" "$temporary/unsat.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$trestle" --every-lemma
# SAT input prevents vacuous acceptance of conditional reasons.
"$trim" -f -c "$temporary/sat.cnf" "$temporary/sat.dsr" "$temporary/sat.lsr" > "$temporary/sat-trim.log" 2>&1
grep -q '^s VALID$' "$temporary/sat-trim.log"
"$trestle" "$temporary/sat.cnf" "$temporary/sat.lsr" c > "$temporary/sat-trestle.log" 2>&1
grep -q '^s VALID$' "$temporary/sat-trestle.log"
"$model" "$temporary/sat.cnf" "$temporary/sat.model"
# Known model has x1=false: this unsupported unconditional unit must decline.
printf '1 0\n' > "$temporary/bad.dsr"
if "$trim" -f -c -U "$temporary/sat.cnf" "$temporary/bad.dsr" "$temporary/bad.lsr" > "$temporary/bad.log" 2>&1; then
  echo 'Unsupported unconditional unit was accepted'
  exit 1
fi
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/bad.log"
python3 "$root/tools/bench/proof_oracle.py" check \
  "$temporary/unsat-drat.cnf" "$temporary/unsat-drat.drat" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$trestle" \
  --drat-trim "$drat" --cake-lpr "$cake"
drat_status=0
"$drat" "$temporary/sat-drat.cnf" "$temporary/sat-drat.drat" -f > "$temporary/sat-drat.log" 2>&1 || drat_status=$?
[[ "$drat_status" -eq 1 ]]
tr -d '\r' < "$temporary/sat-drat.log" | grep -q '^s DERIVATION$'
grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$temporary/sat-drat.log"
"$model" "$temporary/sat-drat.cnf" "$temporary/sat-drat.model"
if "$drat" "$temporary/sat-drat.cnf" "$temporary/bad.dsr" -f -U > "$temporary/bad-drat.log" 2>&1; then
  echo 'Unsupported unconditional DRAT unit was accepted'
  exit 1
fi
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR' "$temporary/bad-drat.log"
# Paused-owner resumption uses the exact union of staged original inputs.
# This is SAT, so the root consequence cannot pass vacuously via an empty line.
printf '2 0\n' > "$temporary/paused-bad.dsr"
for stem in paused paused-dsr-binary; do
  "$trim" -f -c "$temporary/$stem.cnf" "$temporary/$stem.dsr" "$temporary/$stem.lsr" > "$temporary/$stem-trim.log" 2>&1
  grep -q '^s VALID$' "$temporary/$stem-trim.log"
  "$trestle" "$temporary/$stem.cnf" "$temporary/$stem.lsr" c > "$temporary/$stem-trestle.log" 2>&1
  grep -q '^s VALID$' "$temporary/$stem-trestle.log"
  "$model" "$temporary/$stem.cnf" "$temporary/$stem.model"
  if "$trim" -f -c -U "$temporary/$stem.cnf" "$temporary/paused-bad.dsr" "$temporary/$stem-bad.lsr" > "$temporary/$stem-bad.log" 2>&1; then
    echo 'Unsupported paused-owner unit was accepted'
    exit 1
  fi
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/$stem-bad.log"
done
for stem in paused-drat paused-drat-binary; do
  drat_status=0
  "$drat" "$temporary/$stem.cnf" "$temporary/$stem.drat" -f > "$temporary/$stem-check.log" 2>&1 || drat_status=$?
  [[ "$drat_status" -eq 1 ]]
  tr -d '\r' < "$temporary/$stem-check.log" | grep -q '^s DERIVATION$'
  grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$temporary/$stem-check.log"
  "$model" "$temporary/$stem.cnf" "$temporary/$stem.model"
  if "$drat" "$temporary/$stem.cnf" "$temporary/paused-bad.dsr" -f -U > "$temporary/$stem-bad.log" 2>&1; then
    echo 'Unsupported paused-owner DRAT unit was accepted'
    exit 1
  fi
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR' "$temporary/$stem-bad.log"
done
echo 'default_affine_search_proof_model OK'
