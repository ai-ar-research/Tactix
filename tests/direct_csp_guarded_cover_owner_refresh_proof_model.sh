#!/usr/bin/env bash
set -euo pipefail
TEST="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
CHECKMODEL="$ROOT/tools/fuzz/checkmodel"
for checker in "$DSR_TRIM" "$SR_CHECK" "$CHECKMODEL"; do
  [ -x "$checker" ] || exit 77
done
temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
"$TEST" "$temporary/refresh"
for variant in positive signed; do
  stem="$temporary/refresh-$variant"
  "$DSR_TRIM" -f -c -U "$stem.cnf" "$stem.dsr" "$stem.binary.lsr" > "$stem.trim.log" 2>&1
  grep -q '^s VALID$' "$stem.trim.log"
  "$SR_CHECK" "$stem.cnf" "$stem.binary.lsr" c > "$stem.independent.log" 2>&1
  grep -q '^s VALID$' "$stem.independent.log"
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
  if "$DSR_TRIM" -f -c -U "$stem.cnf" "$stem.bad.dsr" "$stem.bad.lsr" > "$stem.bad.log" 2>&1; then
    echo 'False refresh-prefix refutation unexpectedly accepted'
    exit 1
  fi
  grep -Ei 'not RUP|invalid|error|failed' "$stem.bad.log"
done
echo 'direct_csp_guarded_cover_owner_refresh_proof_model OK'
