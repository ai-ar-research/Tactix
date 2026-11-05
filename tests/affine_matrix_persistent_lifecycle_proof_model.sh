#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
"$binary" "$temporary/lifecycle"
count=0
for cnf in "$temporary"/lifecycle-*.cnf; do
  base=${cnf%.cnf}
  "$CHECKMODEL" "$cnf" "$base.model"
  dsr_valid "$cnf" "$base.proof" "$base.lsr" "$base.trim.log" "$base.trestle.log"
  # No original variable is root-forced. RUP-only checking must reject an
  # unsupported unconditional unit, independent of the positive proof stream.
  printf '%s\n' '-1 0' > "$base.bad.dsr"
  reject "$base.bad.log" 'Unsupported unconditional unit was accepted' \
    "$DSR_TRIM" -f -c -U "$cnf" "$base.bad.dsr" "$base.bad.lsr"
  grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$base.bad.log"
  count=$((count + 1))
done
[[ "$count" -eq 8 ]]
# Root maintenance after a root Sweep refutes the XOR system (am-rel-23).
# ProofFormat values: 0 binary DSR, 1 text DSR. The text proof also takes the
# full oracle route through DRAT and CakeLPR.
[[ -x "$DSR_COMPRESS" && -x "$DRAT_TRIM" && -x "$CAKE_LPR" ]] || exit 77
refutations=0
for cnf in "$temporary"/lifecycle.rootmaint-*.cnf; do
  base=${cnf%.cnf}
  format=${base##*-}
  case "$format" in
    0) check_dsr "$cnf" "$base.proof" > "$base.oracle.log" 2>&1 ;;
    1) check_dsr_full "$cnf" "$base.proof" > "$base.oracle.log" 2>&1 ;;
    *) exit 1 ;;
  esac
  grep -q '^s VERIFIED UNSAT$' "$base.oracle.log"
  refutations=$((refutations + 1))
done
[[ "$refutations" -eq 2 ]]
echo 'affine_matrix_persistent_lifecycle_proof_model OK: 8 original models and proof prefixes, 2 root-maintenance refutations'
