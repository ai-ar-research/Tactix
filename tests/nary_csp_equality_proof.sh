#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
EQUALITY_TEST="$1"
checker_paths "$2"
require_tools "native DSR checker pipeline not built — skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"
make_scratch temporary
stem="$temporary/nary-equality"
"$EQUALITY_TEST" "$stem"
check_dsr "$stem.cnf" "$stem.dsr"
echo "nary_csp_equality_proof OK"
