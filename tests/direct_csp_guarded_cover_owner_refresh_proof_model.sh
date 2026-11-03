#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
TEST="$1"
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
"$TEST" "$temporary/refresh"
for variant in positive signed; do
  stem="$temporary/refresh-$variant"
  dsr_valid "$stem.cnf" "$stem.dsr" "$stem.binary.lsr" \
    "$stem.trim.log" "$stem.independent.log" -U
  python3 - "$variant" "$stem.model" <<'PY'
import sys
positive = {3, 5 if sys.argv[1] == 'positive' else 4, 10, 11}
values = [v if v in positive else -v for v in range(1, 13)]
with open(sys.argv[2], 'w') as stream:
    stream.write('s SATISFIABLE\nv ' + ' '.join(map(str, values)) + ' 0\n')
PY
  "$CHECKMODEL" "$stem.cnf" "$stem.model"
  # Reject a false refutation appended to the actual accepted refresh prefix.
  cp "$stem.dsr" "$stem.bad.dsr"
  printf '0\n' >> "$stem.bad.dsr"
  reject "$stem.bad.log" 'False refresh-prefix refutation unexpectedly accepted' \
    "$DSR_TRIM" -f -c -U "$stem.cnf" "$stem.bad.dsr" "$stem.bad.lsr"
  grep -Ei 'not RUP|invalid|error|failed' "$stem.bad.log"
done
echo 'direct_csp_guarded_cover_owner_refresh_proof_model OK'
