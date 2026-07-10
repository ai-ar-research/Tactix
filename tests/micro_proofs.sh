#!/usr/bin/env bash
# Micro-formula proof tests (rule-dev skill, definition of done #2): tiny
# UNSAT formulas through the real binary; DRAT accepted by drat-trim in
# both binary and ascii encodings. Exit 77 (ctest SKIP) without a checker.
set -euo pipefail
TACTIX="$1"
ROOT="$2"
DRAT_TRIM="$ROOT/tools/checkers/bin/drat-trim"
[ -x "$DRAT_TRIM" ] || { echo "drat-trim not built — skipping"; exit 77; }

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

make_cases() {
  # contradictory units
  printf 'p cnf 1 2\n1 0\n-1 0\n' > "$T/units.cnf"
  # classic 2-var full enumeration
  printf 'p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n' > "$T/enum2.cnf"
  # pigeonhole PHP(3): 4 pigeons, 3 holes
  python3 - "$T/php3.cnf" <<'EOF'
import sys
holes, pigeons = 3, 4
var = lambda p, h: p * holes + h + 1
cls = [[var(p, h) for h in range(holes)] for p in range(pigeons)]
for h in range(holes):
    for p1 in range(pigeons):
        for p2 in range(p1 + 1, pigeons):
            cls.append([-var(p1, h), -var(p2, h)])
with open(sys.argv[1], "w") as f:
    f.write(f"p cnf {pigeons * holes} {len(cls)}\n")
    for c in cls:
        f.write(" ".join(map(str, c)) + " 0\n")
EOF
  # empty clause in input
  printf 'p cnf 2 2\n1 2 0\n0\n' > "$T/empty.cnf"
  # cyclic xor chain, odd total parity (UNSAT):
  # 1^2=1, 2^3=1, 1^3=1 sums to 0=1
  printf 'p cnf 3 6\n1 2 0\n-1 -2 0\n2 3 0\n-2 -3 0\n1 3 0\n-1 -3 0\n' \
    > "$T/xor3.cnf"
}

make_cases
for cnf in "$T"/*.cnf; do
  for mode in binary ascii; do
    proof="$cnf.$mode.drat"
    if [ "$mode" = binary ]; then
      "$TACTIX" -q "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    else
      "$TACTIX" -q --no-binary "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    fi
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT (rc=$rc)"; exit 1; }
    grep -q '^s UNSATISFIABLE$' "$T/out" \
      || { echo "FAIL: $cnf missing s line"; exit 1; }
    # judge by output, not exit code: drat-trim exits nonzero on
    # "trivial UNSAT" even when it prints s VERIFIED
    "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
    grep -q 's VERIFIED' "$T/check" \
      || { echo "FAIL: proof of $cnf ($mode) rejected"; cat "$T/check"; exit 1; }
  done
done
echo "micro_proofs OK"
