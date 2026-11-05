#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
ACTION_TEST="$1"
checker_paths "$2"
require_tools "DirectCSP proof/model checker pipeline not built — skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
"$ACTION_TEST" \
  "$temporary/unsat.cnf" "$temporary/proof.dsr" \
  "$temporary/sat.cnf" "$temporary/model.out"
check_dsr "$temporary/unsat.cnf" "$temporary/proof.dsr"
"$CHECKMODEL" "$temporary/sat.cnf" "$temporary/model.out"
echo "direct_csp_support_proof_model OK"
