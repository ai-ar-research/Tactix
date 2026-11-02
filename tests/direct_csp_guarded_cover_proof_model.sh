#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
ACTION_TEST="$1"
checker_paths "$2"
require_tools "GuardedCoverV2 proof/model checker pipeline not built — skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$DRAT_TRIM" "$CHECKMODEL"
make_scratch temporary
"$ACTION_TEST" \
  "$temporary/unsat.cnf" "$temporary/proof.dsr" \
  "$temporary/proof.drat" "$temporary/sat.cnf" "$temporary/model.out" \
  "$temporary/prefix.dsr" "$temporary/prefix.drat" "$temporary/bad.drat"
check_dsr "$temporary/unsat.cnf" "$temporary/proof.dsr"
# Independently authenticate both nonrefuting native lemmas against SAT input.
dsr_valid "$temporary/sat.cnf" "$temporary/prefix.dsr" "$temporary/prefix.lsr" \
  "$temporary/prefix-dsr.log" "$temporary/prefix-trestle.log" -U
# drat-trim deliberately returns 1 for a valid nonrefuting derivation.
drat_derivation "$temporary/prefix-drat.log" \
  "$temporary/sat.cnf" "$temporary/prefix.drat" -f -U
reject "$temporary/bad.log" "Unsupported tuple was unexpectedly accepted" \
  "$DRAT_TRIM" "$temporary/sat.cnf" "$temporary/bad.drat" -f -U
grep -Eq 'NOT VERIFIED|INVALID|failed' "$temporary/bad.log"
# DSR's pure RUP spelling is identical for this unsupported tuple.
reject "$temporary/bad-dsr.log" "Unsupported DSR RUP tuple was unexpectedly accepted" \
  "$DSR_TRIM" -f -c -U "$temporary/sat.cnf" "$temporary/bad.drat" "$temporary/bad.lsr"
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|Clause [0-9]+ is not RUP' "$temporary/bad-dsr.log"
"$DRAT_TRIM" "$temporary/unsat.cnf" "$temporary/proof.drat"
"$CHECKMODEL" "$temporary/sat.cnf" "$temporary/model.out"
echo "direct_csp_guarded_cover_proof_model OK"
