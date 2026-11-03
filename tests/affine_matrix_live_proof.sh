#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
LIVE_TEST="$1"
checker_paths "$2"
require_tools "native DSR checker pipeline not built — skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$VERIPB" "$CAKE_PB"
make_scratch temporary
"$LIVE_TEST" --proof "$temporary/live.cnf" "$temporary/live.dsr"
check_dsr "$temporary/live.cnf" "$temporary/live.dsr"
"$LIVE_TEST" --pbp-proof "$temporary/live-pbp.cnf" "$temporary/live.pbp"
python3 "$PROOF_ORACLE" check \
  "$temporary/live-pbp.cnf" "$temporary/live.pbp" \
  --veripb "$VERIPB" --cake-pb "$CAKE_PB"
echo "affine_matrix_live_proof OK"
