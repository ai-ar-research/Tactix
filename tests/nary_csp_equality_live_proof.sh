#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
LIVE_TEST="$1"
checker_paths "$2"
require_tools "native DSR checker pipeline not built — skipping" \
  "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"
make_scratch temporary
stem="$temporary/nary-equality-live"
"$LIVE_TEST" "$stem"
grep -q 'nary-csp-equality-begin' "$stem.records.jsonl"
grep -q 'omit-exact-aligned-equality' "$stem.records.jsonl"
check_dsr "$stem.cnf" "$stem.dsr"
echo "nary_csp_equality_live_proof OK"
