#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$DRAT_TRIM" "$CAKE_LPR" "$CHECKMODEL"
make_scratch temporary
"$binary" "$temporary"
# Full original-CNF terminal proof plus independently checked every lemma.
check_dsr_full "$temporary/unsat.cnf" "$temporary/unsat.dsr"
check_dsr "$temporary/unsat.cnf" "$temporary/unsat.dsr"
# DefaultPolicy's opt-in schedule must materialize the same deferred reason
# through the public boundary and remain valid against the untouched input.
check_dsr_full "$temporary/deferred-policy.cnf" "$temporary/deferred-policy.dsr"
check_dsr "$temporary/deferred-policy.cnf" "$temporary/deferred-policy.dsr"
# SAT input prevents vacuous acceptance of conditional reasons.
dsr_valid "$temporary/sat.cnf" "$temporary/sat.dsr" "$temporary/sat.lsr" \
  "$temporary/sat-trim.log" "$temporary/sat-trestle.log"
"$CHECKMODEL" "$temporary/sat.cnf" "$temporary/sat.model"
# Known model has x1=false: this unsupported unconditional unit must decline.
printf '1 0\n' > "$temporary/bad.dsr"
reject "$temporary/bad.log" 'Unsupported unconditional unit was accepted' \
  "$DSR_TRIM" -f -c -U "$temporary/sat.cnf" "$temporary/bad.dsr" "$temporary/bad.lsr"
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/bad.log"
check_dsr_full "$temporary/unsat-drat.cnf" "$temporary/unsat-drat.drat"
drat_derivation "$temporary/sat-drat.log" \
  "$temporary/sat-drat.cnf" "$temporary/sat-drat.drat" -f
"$CHECKMODEL" "$temporary/sat-drat.cnf" "$temporary/sat-drat.model"
reject "$temporary/bad-drat.log" 'Unsupported unconditional DRAT unit was accepted' \
  "$DRAT_TRIM" "$temporary/sat-drat.cnf" "$temporary/bad.dsr" -f -U
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR' "$temporary/bad-drat.log"
# Paused-owner resumption uses the exact union of staged original inputs.
# This is SAT, so the root consequence cannot pass vacuously via an empty line.
printf '2 0\n' > "$temporary/paused-bad.dsr"
for stem in paused paused-dsr-binary; do
  dsr_valid "$temporary/$stem.cnf" "$temporary/$stem.dsr" "$temporary/$stem.lsr" \
    "$temporary/$stem-trim.log" "$temporary/$stem-trestle.log"
  "$CHECKMODEL" "$temporary/$stem.cnf" "$temporary/$stem.model"
  reject "$temporary/$stem-bad.log" 'Unsupported paused-owner unit was accepted' \
    "$DSR_TRIM" -f -c -U "$temporary/$stem.cnf" "$temporary/paused-bad.dsr" "$temporary/$stem-bad.lsr"
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/$stem-bad.log"
done
for stem in paused-drat paused-drat-binary; do
  drat_derivation "$temporary/$stem-check.log" \
    "$temporary/$stem.cnf" "$temporary/$stem.drat" -f
  "$CHECKMODEL" "$temporary/$stem.cnf" "$temporary/$stem.model"
  reject "$temporary/$stem-bad.log" 'Unsupported paused-owner DRAT unit was accepted' \
    "$DRAT_TRIM" "$temporary/$stem.cnf" "$temporary/paused-bad.dsr" -f -U
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR' "$temporary/$stem-bad.log"
done
echo 'default_affine_search_proof_model OK'
