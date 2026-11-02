#!/usr/bin/env bash
# DSR check of the restart-trigger fixture's published Gaussian consequences
# against the exact union of its staged inputs (satisfiable, so no lemma can
# pass vacuously through an empty clause).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$SR_CHECK"
make_scratch temporary
"$binary" "$temporary"
grep -qv '^d ' "$temporary/trigger.dsr"
dsr_valid "$temporary/trigger.cnf" "$temporary/trigger.dsr" "$temporary/trigger.lsr" \
  "$temporary/trim.log" "$temporary/trestle.log"
echo 'default_affine_restart_trigger_proof OK'
