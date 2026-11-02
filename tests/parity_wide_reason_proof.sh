#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
PARITY_TEST="$1"
checker_paths "$2"
require_tools "native DSR checker pipeline not built — skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"
make_scratch temporary
"$PARITY_TEST" --wide-reason "$temporary/reason.cnf" "$temporary/reason.dsr"
check_dsr "$temporary/reason.cnf" "$temporary/reason.dsr"
echo "parity_wide_reason_proof OK"
