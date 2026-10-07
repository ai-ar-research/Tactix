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
"$TEST" "$temporary/transitivity"
for arity in 2 3 4; do
  stem="$temporary/transitivity-$arity"
  "$DSR_TRIM" -f -c "$stem.cnf" "$stem.dsr" "$stem.binary.lsr" > "$stem.trim.log" 2>&1
  grep -q '^s VALID$' "$stem.trim.log"
  "$SR_CHECK" "$stem.cnf" "$stem.binary.lsr" c > "$stem.independent.log" 2>&1
  grep -q '^s VALID$' "$stem.independent.log"
  python3 - "$arity" "$stem.model" <<'PY'
import sys
n=int(sys.argv[1])
values=[v if (v-1)%n == (0 if (v-1)//n<3 else 1) else -v for v in range(1,6*n+1)]
open(sys.argv[2],'w').write('s SATISFIABLE\nv '+' '.join(map(str,values))+' 0\n')
PY
  "$CHECKMODEL" "$stem.cnf" "$stem.model"
done
# A false empty-clause addition on satisfiable input must fail elaboration.
printf 'p cnf 2 2\n1 2 0\n-1 2 0\n' > "$temporary/control.cnf"
printf '0\n' > "$temporary/bad.dsr"
if "$DSR_TRIM" -f -c "$temporary/control.cnf" "$temporary/bad.dsr" "$temporary/bad.lsr" > "$temporary/bad.log" 2>&1; then
  echo 'False refutation unexpectedly accepted'
  exit 1
fi
grep -Ei 'not RUP|not RAT|invalid|error|failed' "$temporary/bad.log"
echo 'nary_csp_equality_transitivity_proof_model OK'
