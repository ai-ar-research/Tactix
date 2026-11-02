#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
TEST="$1"
checker_paths "$2"
require_tools "" "$DSR_TRIM" "$SR_CHECK" "$CHECKMODEL"
make_scratch temporary
"$TEST" "$temporary/transitivity"
for arity in 2 3 4; do
  stem="$temporary/transitivity-$arity"
  dsr_valid "$stem.cnf" "$stem.dsr" "$stem.binary.lsr" \
    "$stem.trim.log" "$stem.independent.log"
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
reject "$temporary/bad.log" 'False refutation unexpectedly accepted' \
  "$DSR_TRIM" -f -c "$temporary/control.cnf" "$temporary/bad.dsr" "$temporary/bad.lsr"
grep -Ei 'not RUP|not RAT|invalid|error|failed' "$temporary/bad.log"
echo 'nary_csp_equality_transitivity_proof_model OK'
