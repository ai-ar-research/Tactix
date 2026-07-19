#!/usr/bin/env bash
# Micro-formula proof tests (rule-dev skill, definition of done #2): tiny
# UNSAT formulas through the real binary; DRAT accepted by drat-trim in
# both binary and ascii encodings, native LRAT accepted by lrat-check.
# Exit 77 (ctest SKIP) without a checker.
set -euo pipefail
TACTIX="$1"
ROOT="$2"
DRAT_TRIM="$ROOT/tools/checkers/bin/drat-trim"
LRAT_CHECK="$ROOT/tools/checkers/bin/lrat-check"
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
  # native LRAT (plan O4): hints emitted by the solver, no conversion
  if [ -x "$LRAT_CHECK" ]; then
    proof="$cnf.lrat"
    "$TACTIX" -q --lrat "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under --lrat"; exit 1; }
    "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
      || { echo "FAIL: native LRAT of $cnf rejected"; cat "$T/check"; exit 1; }
    grep -qi 'verif' "$T/check" \
      || { echo "FAIL: lrat-check gave no verdict on $cnf"; cat "$T/check"; exit 1; }
  fi
done

# ---- E1d: the same tiny UNSAT formulas refuted with BVE in the proof ----
# `--bve-stress-policy` maximizes BVE at the root, so these check that BVE's
# resolvent/deletion deltas compose into a valid DRAT + native-LRAT proof:
# enum2 is refuted by BVE alone (the equivalence-gate path derives {b},{¬b}→∅);
# xor3/php3 mix BVE resolvents with a search-proof tail.
for cnf in "$T/enum2.cnf" "$T/xor3.cnf" "$T/php3.cnf"; do
  for mode in binary ascii; do
    proof="$cnf.bve.$mode.drat"
    if [ "$mode" = binary ]; then
      "$TACTIX" -q --bve-stress-policy "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    else
      "$TACTIX" -q --bve-stress-policy --no-binary "$cnf" "$proof" > "$T/out" \
        && rc=$? || rc=$?
    fi
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under BVE (rc=$rc)"; exit 1; }
    grep -q '^s UNSATISFIABLE$' "$T/out" \
      || { echo "FAIL: $cnf (BVE) missing s line"; exit 1; }
    "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
    grep -q 's VERIFIED' "$T/check" \
      || { echo "FAIL: BVE proof of $cnf ($mode) rejected"; cat "$T/check"; exit 1; }
  done
  if [ -x "$LRAT_CHECK" ]; then
    proof="$cnf.bve.lrat"
    "$TACTIX" -q --bve-stress-policy --lrat "$cnf" "$proof" > "$T/out" \
      && rc=$? || rc=$?
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under BVE --lrat"; exit 1; }
    "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
      || { echo "FAIL: native LRAT (BVE) of $cnf rejected"; cat "$T/check"; exit 1; }
    grep -qi 'verif' "$T/check" \
      || { echo "FAIL: lrat-check gave no verdict on BVE $cnf"; cat "$T/check"; exit 1; }
  fi
done
echo "micro_proofs OK"
