#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"
make_scratch tmp
"$binary" --proof-search "$tmp/input.cnf" "$tmp/proof.dsr"
check_dsr "$tmp/input.cnf" "$tmp/proof.dsr"
