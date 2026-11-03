#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
TEST="$1"
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"
make_scratch temporary
stem="$temporary/nary-equality-amo"
"$TEST" "$stem"
grep -q 'nary-csp-equality-amo' "$stem.records.jsonl"
check_dsr "$stem.cnf" "$stem.dsr"
echo "nary_csp_equality_amo_proof OK"
