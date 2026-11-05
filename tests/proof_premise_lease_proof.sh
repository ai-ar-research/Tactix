#!/usr/bin/env bash
# A witnessed SR line after leased clauses were natively deleted, on every route.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$DRAT_TRIM" "$CAKE_LPR" "$VERIPB" "$CAKE_PB"
make_scratch temporary
"$binary" "$temporary/lease"
for route in 0 1 2; do
  python3 "$PROOF_ORACLE" check "$temporary/lease-sr-$route.cnf" "$temporary/lease-sr-$route.proof" \
    --dsr-trim "$DSR_TRIM" --dsr-compress "$DSR_COMPRESS" --srcheck "$SR_CHECK" \
    --drat-trim "$DRAT_TRIM" --cake-lpr "$CAKE_LPR" --veripb "$VERIPB" --cake-pb "$CAKE_PB" |
    grep -q 's VERIFIED UNSAT'
done
