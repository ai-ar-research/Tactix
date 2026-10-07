#!/usr/bin/env bash
set -euo pipefail

ACTION_TEST="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
DRAT_TRIM="${DRAT_TRIM:-$ROOT/tools/checkers/bin/drat-trim}"
CHECKMODEL="$ROOT/tools/fuzz/checkmodel"

for checker in "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$DRAT_TRIM" "$CHECKMODEL"; do
  [ -x "$checker" ] || {
    echo "GuardedCoverV2 proof/model checker pipeline not built — skipping"
    exit 77
  }
done

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
"$ACTION_TEST" \
  "$temporary/unsat.cnf" "$temporary/proof.dsr" \
  "$temporary/proof.drat" "$temporary/sat.cnf" "$temporary/model.out" \
  "$temporary/prefix.dsr" "$temporary/prefix.drat" "$temporary/bad.drat"

python3 "$ROOT/tools/bench/proof_oracle.py" check-dsr \
  "$temporary/unsat.cnf" "$temporary/proof.dsr" \
  --dsr-trim "$DSR_TRIM" \
  --dsr-compress "$DSR_COMPRESS" \
  --srcheck "$SR_CHECK" \
  --every-lemma
# Independently authenticate both nonrefuting native lemmas against SAT input.
"$DSR_TRIM" -f -c -U "$temporary/sat.cnf" "$temporary/prefix.dsr" "$temporary/prefix.lsr" > "$temporary/prefix-dsr.log" 2>&1
grep -q '^s VALID$' "$temporary/prefix-dsr.log"
"$SR_CHECK" "$temporary/sat.cnf" "$temporary/prefix.lsr" c > "$temporary/prefix-trestle.log" 2>&1
grep -q '^s VALID$' "$temporary/prefix-trestle.log"
drat_status=0
"$DRAT_TRIM" "$temporary/sat.cnf" "$temporary/prefix.drat" -f -U > "$temporary/prefix-drat.log" 2>&1 || drat_status=$?
# drat-trim deliberately returns 1 for a valid nonrefuting derivation.
[ "$drat_status" -eq 1 ]
tr -d '\r' < "$temporary/prefix-drat.log" | grep -q '^s DERIVATION$'
grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$temporary/prefix-drat.log"
if "$DRAT_TRIM" "$temporary/sat.cnf" "$temporary/bad.drat" -f -U > "$temporary/bad.log" 2>&1; then
  echo "Unsupported tuple was unexpectedly accepted"
  exit 1
fi
grep -Eq 'NOT VERIFIED|INVALID|failed' "$temporary/bad.log"
# DSR's pure RUP spelling is identical for this unsupported tuple.
if "$DSR_TRIM" -f -c -U "$temporary/sat.cnf" "$temporary/bad.drat" "$temporary/bad.lsr" > "$temporary/bad-dsr.log" 2>&1; then
  echo "Unsupported DSR RUP tuple was unexpectedly accepted"
  exit 1
fi
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|Clause [0-9]+ is not RUP' "$temporary/bad-dsr.log"
"$DRAT_TRIM" "$temporary/unsat.cnf" "$temporary/proof.drat"
"$CHECKMODEL" "$temporary/sat.cnf" "$temporary/model.out"

echo "direct_csp_guarded_cover_proof_model OK"
