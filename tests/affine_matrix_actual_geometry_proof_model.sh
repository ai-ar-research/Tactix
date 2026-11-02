#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
"$binary" "$temporary/geometry"
dsr_valid "$temporary/geometry.cnf" "$temporary/geometry.dsr" "$temporary/geometry.lsr" \
  "$temporary/trim.log" "$temporary/trestle.log"
"$CHECKMODEL" "$temporary/geometry.cnf" "$temporary/geometry.model"
# The planted model makes x1 false: this unit is not an unconditional consequence.
printf '1 0\n' > "$temporary/bad.dsr"
reject "$temporary/bad.log" 'Unsupported unconditional geometry unit was accepted' \
  "$DSR_TRIM" -f -c -U "$temporary/geometry.cnf" "$temporary/bad.dsr" "$temporary/bad.lsr"
grep -Eq 'NOT VERIFIED|INVALID|failed|ERROR|is not RUP' "$temporary/bad.log"
echo 'affine_matrix_actual_geometry_proof_model OK'
