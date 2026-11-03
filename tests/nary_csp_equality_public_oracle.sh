#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
test_binary="$1"
checker_paths "$2"
require_tools "proof/model checker pipeline not built - skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
stem="$temporary/nary-equality-public"
"$test_binary" "$stem"
check_dsr "$stem.cnf" "$stem.dsr"
"$CHECKMODEL" "$stem.sat.cnf" "$stem.model"
grep -q 'nary-csp-equality-begin' "$stem.records.jsonl"
echo "nary_csp_equality_public_oracle OK"
