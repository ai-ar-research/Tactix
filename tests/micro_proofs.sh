#!/usr/bin/env bash
# Micro-formula proof tests (N2): tiny UNSAT formulas through the real binary;
# DRAT accepted by drat-trim in both binary and ascii encodings, native LRAT
# accepted by lrat-check. Exit 77 (ctest SKIP) without a checker.
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
  # Input normalization: a repeated literal makes the stored clause differ from
  # the one the checker reads at that file position, so every chain citing its
  # id needs a restatement lemma first (frontend/parse.cpp finish_input). One
  # case per stored width; each has its own re-pointing target.
  printf 'p cnf 3 4\n-1 -1 0\n-1 2 3 0\n-2 0\n1 1 0\n' > "$T/dupunit.cnf"
  printf 'p cnf 5 6\n5 1 0\n-5 4 0\n5 -1 0\n-4 3 3 0\n-2 0\n-4 -3 0\n' \
    > "$T/dupbinary.cnf"
  printf 'p cnf 4 8\n1 1 2 3 0\n-2 0\n-3 0\n-1 4 0\n-1 -4 0\n2 3 4 0\n-4 2 0\n-4 3 0\n' \
    > "$T/duplarge.cnf"
  # A prior root unit shrinks a later input binary to a unit. The stored unit
  # needs a deferred lemma with both the prior unit and file clause as hints;
  # the following opposite unit then cites that restated id in the empty proof.
  printf 'p cnf 2 3\n1 0\n-1 2 0\n-2 0\n' > "$T/rootshrink.cnf"
  # Startup sparse collection: the large clause precedes the binary implication
  # that fixes x1, so input normalization cannot shorten it. Initial propagation
  # derives x1; the collector then rewrites the large clause to binary (5 6).
  printf 'p cnf 6 7\n-1 5 6 0\n-2 1 0\n2 0\n3 4 0\n3 -4 0\n-3 4 0\n-3 -4 0\n' \
    > "$T/sparsecollect.cnf"
  # a refutation found DURING input, with input clauses still to come: the
  # derived empty clause must not take an id the checker gave to clause 3 or 4
  printf 'p cnf 2 4\n1 0\n-1 0\n2 0\n-2 0\n' > "$T/midrefute.cnf"
  # cyclic xor chain, odd total parity: 1^2=1, 2^3=1, 1^3=1 sums to 0=1
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
    # "trivial UNSAT" even when it verifies
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
    grep -q 'c VERIFIED' "$T/check" \
      || { echo "FAIL: lrat-check gave no verdict on $cnf"; cat "$T/check"; exit 1; }
  fi
done

# ---- the same formulas with BVE in the proof. The generic stress scenario
# BVE at the root, so BVE's resolvent/deletion deltas must compose into a valid
# DRAT + native-LRAT proof: enum2 is refuted by BVE alone (the equivalence-gate
# path derives {b},{¬b}→∅), xor3/php3 mix resolvents with a search-proof tail.
for cnf in "$T/enum2.cnf" "$T/xor3.cnf" "$T/php3.cnf"; do
  for mode in binary ascii; do
    proof="$cnf.bve.$mode.drat"
    if [ "$mode" = binary ]; then
      TACTIX_STRESS_RULES=BVE "$TACTIX" -q --stress-policy "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    else
      TACTIX_STRESS_RULES=BVE "$TACTIX" -q --stress-policy --no-binary "$cnf" "$proof" > "$T/out" \
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
    TACTIX_STRESS_RULES=BVE "$TACTIX" -q --stress-policy --lrat "$cnf" "$proof" > "$T/out" \
      && rc=$? || rc=$?
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under BVE --lrat"; exit 1; }
    "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
      || { echo "FAIL: native LRAT (BVE) of $cnf rejected"; cat "$T/check"; exit 1; }
    grep -q 'c VERIFIED' "$T/check" \
      || { echo "FAIL: lrat-check gave no verdict on BVE $cnf"; cat "$T/check"; exit 1; }
  fi
done
# ---- BVE through a general definition. `--bve-gates=def` masks off every
# syntactic shape, so any gate here came from the sub-solver and the resolvents
# include non-tautological gate×gate pairs -- the `resolve_gate` path nothing
# else here reaches. The counter assertion below proves that reachability.
printf 'p cnf 4 8\n1 2 0\n1 3 0\n-1 -2 4 0\n-1 -3 -4 0\n2 3 0\n-2 -3 0\n-2 -4 0\n-3 4 0\n' \
  > "$T/bvedef.cnf"
cnf="$T/bvedef.cnf"
for mode in binary ascii; do
  proof="$cnf.$mode.drat"
  extra=""
  [ "$mode" = ascii ] && extra="--no-binary"
  # shellcheck disable=SC2086
  TACTIX_STRESS_RULES=BVE "$TACTIX" --stress-policy --bve-gates=def $extra "$cnf" "$proof" \
    > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] || { echo "FAIL: bvedef not UNSAT (rc=$rc)"; exit 1; }
  grep -qE '^c bve-gates .*[1-9][0-9]* definition' "$T/out" \
    || { echo "FAIL: no definition gate extracted — the case is vacuous"; \
         grep '^c bve' "$T/out"; exit 1; }
  "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
  grep -q 's VERIFIED' "$T/check" \
    || { echo "FAIL: definition-gate proof ($mode) rejected"; cat "$T/check"; \
         exit 1; }
done
if [ -x "$LRAT_CHECK" ]; then
  proof="$cnf.lrat"
  TACTIX_STRESS_RULES=BVE "$TACTIX" -q --stress-policy --bve-gates=def --lrat "$cnf" "$proof" \
    > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] || { echo "FAIL: bvedef not UNSAT under --lrat"; exit 1; }
  "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
    || { echo "FAIL: native LRAT of the definition gate rejected"; \
         cat "$T/check"; exit 1; }
  grep -q 'c VERIFIED' "$T/check" \
    || { echo "FAIL: lrat-check gave no verdict on bvedef"; cat "$T/check"; \
         exit 1; }
fi

# ---- Factor (BVA) native-LRAT: dividers emit vacuous add-RAT (fresh pivot, no
# ¬x clauses yet → empty hints) and quotients emit add-RAT with one resolution
# group per divider (the resolvent (fᵢ∨rⱼ) discharged by the still-live
# original's id). The second case duplicates a grid binary, for delete-all-copies
# under LRAT. The third is a 2x3 occurrence grid whose three columns have equal
# large-clause contents but distinct ids; it pins aligned multiset hints and
# deletion. TACTIX_NO_BVE keeps BVE off the grid until Factor has run.
printf 'p cnf 7 13\n1 3 0\n1 4 0\n1 5 0\n2 3 0\n2 4 0\n2 5 0\n-1 -2 0\n-3 -4 0\n-5 -3 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n' > "$T/factor.cnf"
printf 'p cnf 7 14\n1 3 0\n1 3 0\n1 4 0\n1 5 0\n2 3 0\n2 4 0\n2 5 0\n-1 -2 0\n-3 -4 0\n-5 -3 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n' > "$T/factor_dup.cnf"
printf 'p cnf 6 10\n1 3 4 0\n2 3 4 0\n1 3 4 0\n2 3 4 0\n1 3 4 0\n2 3 4 0\n5 6 0\n5 -6 0\n-5 6 0\n-5 -6 0\n' > "$T/factor_dup_large.cnf"
for cnf in "$T/factor.cnf" "$T/factor_dup.cnf" "$T/factor_dup_large.cnf"; do
  for mode in binary ascii; do
    proof="$cnf.fac.$mode.drat"
    if [ "$mode" = binary ]; then
      TACTIX_NO_BVE=1 "$TACTIX" -q "$cnf" "$proof" \
        > "$T/out" && rc=$? || rc=$?
    else
      TACTIX_NO_BVE=1 "$TACTIX" -q --no-binary "$cnf" \
        "$proof" > "$T/out" && rc=$? || rc=$?
    fi
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under Factor (rc=$rc)"; exit 1; }
    grep -q '^s UNSATISFIABLE$' "$T/out" \
      || { echo "FAIL: $cnf (Factor) missing s line"; exit 1; }
    "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
    grep -q 's VERIFIED' "$T/check" \
      || { echo "FAIL: Factor proof of $cnf ($mode) rejected"; cat "$T/check"; exit 1; }
  done
  if [ -x "$LRAT_CHECK" ]; then
    proof="$cnf.fac.lrat"
    TACTIX_NO_BVE=1 "$TACTIX" -q --lrat "$cnf" "$proof" \
      > "$T/out" && rc=$? || rc=$?
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under Factor --lrat"; exit 1; }
    "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
      || { echo "FAIL: native LRAT (Factor) of $cnf rejected"; cat "$T/check"; exit 1; }
    grep -q 'c VERIFIED' "$T/check" \
      || { echo "FAIL: lrat-check gave no verdict on Factor $cnf"; cat "$T/check"; exit 1; }
  fi
done
echo "micro_proofs OK"
