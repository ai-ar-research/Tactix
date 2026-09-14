#!/usr/bin/env bash
# Micro-formula proof tests (N2): tiny UNSAT formulas through the real binary;
# canonical DSR accepted by dsr-trim and pinned Trestle in binary and ASCII,
# with explicit DRAT/native-LRAT compatibility coverage. Exit 77 (ctest SKIP)
# without the canonical checker pipeline.
set -euo pipefail
TACTIX="$1"
ROOT="$2"
DRAT_TRIM="$ROOT/tools/checkers/bin/drat-trim"
LRAT_CHECK="$ROOT/tools/checkers/bin/lrat-check"
CAKE_LPR="$ROOT/tools/checkers/bin/cake_lpr"
DSR_TRIM="$ROOT/tools/checkers/bin/dsr-trim"
DSR_COMPRESS="$ROOT/tools/checkers/bin/dsr-compress"
SR_CHECK="$ROOT/tools/checkers/bin/srcheck"
ANALYZE_TEST="${3:-}"
CONGRUENCE_TEST="${4:-}"
RESTART_TEST="${5:-}"
FACTOR_N1_TEST="${6:-}"
ONLINE_INPROCESS_TEST="${7:-}"
for checker in "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"; do
  [ -x "$checker" ] \
    || { echo "native DSR checker pipeline not built — skipping"; exit 77; }
done

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

check_dsr() {
  local cnf="$1" proof="$2" direction="${3:-forward}"
  local binary_lsr="$T/check.binary.lsr" text_lsr="$T/check.lsr"
  local direction_args=()
  [ "$direction" = forward ] && direction_args=(-f)
  rm -f "$binary_lsr" "$text_lsr"
  "$DSR_TRIM" -q -c "${direction_args[@]}" "$cnf" "$proof" "$binary_lsr" \
    > "$T/dsr-trim.out" 2>&1 \
    || { echo "FAIL: dsr-trim rejected $proof"; cat "$T/dsr-trim.out"; return 1; }
  grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/dsr-trim.out" \
    || { echo "FAIL: dsr-trim gave no verified verdict for $proof"; \
         cat "$T/dsr-trim.out"; return 1; }
  [ -s "$binary_lsr" ] \
    || { echo "FAIL: dsr-trim emitted no LSR for $proof"; return 1; }
  "$DSR_COMPRESS" -d "$binary_lsr" "$text_lsr" \
    > "$T/dsr-compress.out" 2>&1 \
    || { echo "FAIL: DSR LSR decompression failed for $proof"; \
         cat "$T/dsr-compress.out"; return 1; }
  [ -s "$text_lsr" ] \
    || { echo "FAIL: DSR LSR decompression was empty for $proof"; return 1; }
  PYTHONPATH="${TACTIX_PROOF_ORACLE_DIR:-$ROOT/tools/bench}" \
    python3 - "$cnf" "$text_lsr" <<'PY'
import sys
from proof_oracle import bridge_trivial_input_lsr
bridge_trivial_input_lsr(sys.argv[1], sys.argv[2])
PY
  "$SR_CHECK" "$cnf" "$text_lsr" > "$T/srcheck.out" 2>&1 \
    || { echo "FAIL: Trestle rejected $proof"; cat "$T/srcheck.out"; return 1; }
  grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/srcheck.out" \
    || { echo "FAIL: Trestle gave no verified verdict for $proof"; \
         cat "$T/srcheck.out"; return 1; }
}

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
  # A unit implication cascade is closed while DRAT input is still being read.
  # Its derived x3 shrinks a later ternary to binary, and the following units
  # refute during ingestion.  This exercises derived-root normalization without
  # LRAT ids; native LRAT deliberately keeps the deferred-input path.
  printf 'p cnf 5 6\n-1 2 0\n-2 3 0\n1 0\n-3 4 5 0\n-4 0\n-5 0\n' \
    > "$T/rootcascade.cnf"
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
  # TransitiveReduce failed-source unit: assuming -1 reaches both 4 and -4.
  # The independent full enumeration on 5,6 keeps the whole formula UNSAT so
  # an external checker can replay the new unit chain and the search tail.
  printf 'p cnf 6 8\n1 2 0\n1 3 0\n-2 4 0\n-2 -4 0\n5 6 0\n5 -6 0\n-5 6 0\n-5 -6 0\n' \
    > "$T/transitive-failed.cnf"
}

make_cases
for cnf in "$T"/*.cnf; do
  for mode in binary ascii; do
    proof="$cnf.$mode.dsr"
    if [ "$mode" = binary ]; then
      "$TACTIX" -q "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    else
      "$TACTIX" -q --no-binary "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    fi
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT (rc=$rc)"; exit 1; }
    grep -q '^s UNSATISFIABLE$' "$T/out" \
      || { echo "FAIL: $cnf missing s line"; exit 1; }
    check_dsr "$cnf" "$proof"
  done
  # The historical proof vocabulary remains available explicitly in both
  # DRAT encodings. It is a compatibility surface, not the default.
  if [ -x "$DRAT_TRIM" ]; then
    for mode in binary ascii; do
      proof="$cnf.$mode.drat"
      if [ "$mode" = binary ]; then
        "$TACTIX" -q --drat "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
      else
        "$TACTIX" -q --drat --no-binary "$cnf" "$proof" \
          > "$T/out" && rc=$? || rc=$?
      fi
      [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under DRAT (rc=$rc)"; exit 1; }
      "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
      grep -q 's VERIFIED' "$T/check" \
        || { echo "FAIL: DRAT proof of $cnf ($mode) rejected"; cat "$T/check"; exit 1; }
    done
  fi
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

# Direct substitution witness: a two-row permutation adds unit -1 to the
# symmetric component. The independent no-UP refutation tail ends the proof.
# Forward checking forces dsr-trim to validate the SR line even though a
# backward trimmer could discard it as irrelevant to the final empty clause.
cat > "$T/direct-sr.cnf" <<'EOF'
p cnf 6 8
1 2 0
3 4 0
-1 -3 0
-2 -4 0
5 6 0
5 -6 0
-5 6 0
-5 -6 0
EOF
cat > "$T/direct-sr.dsr" <<'EOF'
-1 -1 3 -1 2 4 4 2 0
5 0
0
EOF
check_dsr "$T/direct-sr.cnf" "$T/direct-sr.dsr" forward

# Exercise the solver's typed SR application and native DSR writer together.
# The emitted certificate must contain an effective orbitopal transition and
# survive forward elaboration, which prevents backward trimming from hiding a
# malformed or ineffective SR line.
if [ -n "$ONLINE_INPROCESS_TEST" ]; then
  proof="$T/orbitopal-sr.dsr"
  "$ONLINE_INPROCESS_TEST" "$T/direct-sr.cnf" "$proof" dsr OrbitopalSR \
    > "$T/orbitopal-sr.out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] \
    || { echo "FAIL: orbitopal SR fixture not UNSAT (rc=$rc)"; \
         cat "$T/orbitopal-sr.out"; exit 1; }
  grep -Eq '^c effective orbitopal SR transitions [1-9][0-9]*$' \
    "$T/orbitopal-sr.out" \
    || { echo "FAIL: orbitopal SR fixture made no effective transition"; \
         cat "$T/orbitopal-sr.out"; exit 1; }
  check_dsr "$T/direct-sr.cnf" "$proof" forward
fi

# Removing half of the row permutation is not an SR witness. The elaborator
# must reject it before producing an accepted LSR certificate.
cat > "$T/direct-sr-bad.dsr" <<'EOF'
-1 -1 3 -1 2 4 0
5 0
0
EOF
if "$DSR_TRIM" -q -f -c "$T/direct-sr.cnf" "$T/direct-sr-bad.dsr" \
     "$T/direct-sr-bad.lsr" > "$T/direct-sr-bad.out" 2>&1; then
  echo "FAIL: dsr-trim accepted the incomplete substitution witness"
  cat "$T/direct-sr-bad.out"
  exit 1
fi
grep -q 'No UP contradiction' "$T/direct-sr-bad.out" \
  || { echo "FAIL: dsr-trim did not diagnose the invalid SR control"; \
       cat "$T/direct-sr-bad.out"; exit 1; }

# Keep the verified endpoint live as an oracle too: an incomplete final hint
# chain must not be accepted by Trestle.
cat > "$T/direct-sr-bad.lsr" <<'EOF'
9 -1 -1 3 2 -4 0 0
10 5 0 5 6 0
11 0 10 7 0
EOF
"$SR_CHECK" "$T/direct-sr.cnf" "$T/direct-sr-bad.lsr" \
  > "$T/direct-sr-bad-check.out" 2>&1 || true
grep -Eq '^s INVALID$' "$T/direct-sr-bad-check.out" \
  || { echo "FAIL: Trestle gave no rejection for the invalid LSR control"; \
       cat "$T/direct-sr-bad-check.out"; exit 1; }
if grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/direct-sr-bad-check.out"; then
  echo "FAIL: Trestle reported VERIFIED for the invalid LSR control"
  cat "$T/direct-sr-bad-check.out"
  exit 1
fi

# Drive the failed-source rule path directly and require that the independently
# checked proof actually contains a TransitiveReduce unit, rather than passing
# only because the independent UNSAT component was solved without the rule.
if [ -x "$LRAT_CHECK" ]; then
  cnf="$T/transitive-failed.cnf"
  proof="$cnf.transitive.lrat"
  TACTIX_STRESS_RULES=TransitiveReduce TACTIX_V4_EARLY_EXIT=0 \
    "$TACTIX" --stress-policy --lrat "$cnf" "$proof" > "$T/out" \
    && rc=$? || rc=$?
  [ "$rc" -eq 20 ] \
    || { echo "FAIL: transitive failed-source fixture not UNSAT (rc=$rc)"; exit 1; }
  grep -Eq '^c transitive  .* [1-9][0-9]* units,' "$T/out" \
    || { echo "FAIL: transitive failed-source fixture emitted no unit"; cat "$T/out"; exit 1; }
  "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
    || { echo "FAIL: native LRAT of transitive failed-source unit rejected"; \
         cat "$T/check"; exit 1; }
  grep -q 'c VERIFIED' "$T/check" \
    || { echo "FAIL: lrat-check gave no transitive-unit verdict"; \
         cat "$T/check"; exit 1; }
fi

# Analyze's level-one failed-literal path can emit more than one root unit from
# one conflict.  Drive the exact rule transition in the analysis fixture, then
# check the complete native LRAT proof against the matching original CNF.
if [ -n "$ANALYZE_TEST" ] && [ -x "$LRAT_CHECK" ]; then
  cnf="$T/failed-level-one.cnf"
  printf 'p cnf 5 6\n-1 2 0\n-2 3 0\n-2 4 0\n-3 -4 0\n1 5 0\n1 -5 0\n' \
    > "$cnf"
  proof="$cnf.lrat"
  "$ANALYZE_TEST" "$proof" > "$T/out"
  "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
    || { echo "FAIL: native LRAT of level-one failed batch rejected"; \
         cat "$T/check"; exit 1; }
  grep -q 'c VERIFIED' "$T/check" \
    || { echo "FAIL: lrat-check gave no verdict on level-one failed batch"; \
         cat "$T/check"; exit 1; }
fi

# A full Restart can expose a conflict made entirely of chronological
# level-zero implications while the solver stood above root. Restart must
# discharge that conflict before any root rewrite can retire its antecedent.
if [ -n "$RESTART_TEST" ] && [ -x "$CAKE_LPR" ]; then
  cnf="$T/restart-root-conflict.cnf"
  printf 'p cnf 3 3\n-1 0\n-2 0\n1 2 0\n' > "$cnf"
  proof="$cnf.lrat"
  "$RESTART_TEST" "$proof"
  "$CAKE_LPR" "$cnf" "$proof" > "$T/check" 2>&1 \
    || { echo "FAIL: native LRAT of Restart root conflict rejected by cake_lpr"; \
         cat "$T/check"; exit 1; }
  grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/check" \
    || { echo "FAIL: cake_lpr gave no Restart root-conflict verdict"; \
         cat "$T/check"; exit 1; }
  # Keep the oracle live: the same empty lemma with an incomplete hint chain
  # must be rejected.  This catches verdict parsing and checker invocation
  # mistakes in addition to checking the positive proof.
  printf '4 0 1 3 0\n' > "$T/restart-root-conflict-bad.lrat"
  "$CAKE_LPR" "$cnf" "$T/restart-root-conflict-bad.lrat" \
    > "$T/check-bad" 2>&1 || true
  if grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/check-bad"; then
    echo "FAIL: cake_lpr accepted invalid Restart root-conflict control"
    cat "$T/check-bad"
    exit 1
  fi
  grep -q 'Checking failed at' "$T/check-bad" \
    || { echo "FAIL: cake_lpr gave no rejection for invalid Restart control"; \
         cat "$T/check-bad"; exit 1; }
fi

# Congruence's additive semantic surface must carry the same native proof
# obligations as its canonical closure. Each fixture has an independent enum2
# refutation tail, while the helper asserts that it first applied exactly the
# requested V6a unit, gate-degeneracy unit, or unary equivalence.
if [ -n "$CONGRUENCE_TEST" ]; then
  printf 'p cnf 4 6\n1 2 0\n1 -2 0\n3 4 0\n3 -4 0\n-3 4 0\n-3 -4 0\n' \
    > "$T/congruence-v6a.cnf"
  printf 'p cnf 7 10\n-3 4 0\n3 -4 0\n-5 3 4 0\n5 -3 4 0\n5 3 -4 0\n-5 -3 -4 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n' \
    > "$T/congruence-gate-unit.cnf"
  printf 'p cnf 7 9\n-3 4 0\n3 -4 0\n-5 3 0\n-5 4 0\n5 -3 -4 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n' \
    > "$T/congruence-unary.cnf"
  printf 'p cnf 4 8\n-3 1 0\n-3 2 0\n3 -1 -2 0\n-4 1 0\n-4 2 0\n4 -1 -2 0\n-3 -4 0\n3 4 0\n' \
    > "$T/congruence-clash.cnf"
  printf 'p cnf 3 6\n-3 1 0\n-3 2 0\n3 -1 -2 0\n3 1 0\n3 2 0\n-3 -1 -2 0\n' \
    > "$T/congruence-literal-clash.cnf"
  printf 'p cnf 5 12\n-5 1 2 0\n5 -1 2 0\n5 1 -2 0\n-5 -1 -2 0\n5 3 4 0\n-5 -3 4 0\n-5 3 -4 0\n5 -3 -4 0\n-1 3 0\n1 -3 0\n-2 4 0\n2 -4 0\n' \
    > "$T/congruence-literal-xor-clash.cnf"
  printf 'p cnf 7 14\n1 -3 7 0\n1 3 -7 0\n-1 -2 7 0\n-1 2 -7 0\n4 -6 -7 0\n4 6 7 0\n-4 -5 -7 0\n-4 5 7 0\n-1 4 0\n1 -4 0\n-2 5 0\n2 -5 0\n-3 6 0\n3 -6 0\n' \
    > "$T/congruence-literal-ite-clash.cnf"
  for kind in v6a gate-unit unary clash literal-clash \
              literal-xor-clash literal-ite-clash; do
    cnf="$T/congruence-$kind.cnf"
    proof="$cnf.semantic.drat"
    "$CONGRUENCE_TEST" "$cnf" "$proof" drat "$kind" \
      > "$T/out" && rc=$? || rc=$?
    [ "$rc" -eq 20 ] \
      || { echo "FAIL: semantic Congruence $kind DRAT helper (rc=$rc)"; exit 1; }
    "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
    grep -q 's VERIFIED' "$T/check" \
      || { echo "FAIL: semantic Congruence $kind DRAT rejected"; \
           cat "$T/check"; exit 1; }
    if [ -x "$LRAT_CHECK" ]; then
      proof="$cnf.semantic.lrat"
      "$CONGRUENCE_TEST" "$cnf" "$proof" lrat "$kind" \
        > "$T/out" && rc=$? || rc=$?
      [ "$rc" -eq 20 ] \
        || { echo "FAIL: semantic Congruence $kind LRAT helper (rc=$rc)"; exit 1; }
      "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
        || { echo "FAIL: semantic Congruence $kind LRAT rejected"; \
             cat "$T/check"; exit 1; }
      grep -q 'c VERIFIED' "$T/check" \
        || { echo "FAIL: no semantic Congruence $kind LRAT verdict"; \
             cat "$T/check"; exit 1; }
    fi
  done
  for kind in literal-xor-clash literal-ite-clash; do
    cnf="$T/congruence-$kind.cnf"
    for mode in drat lrat; do
      [ "$mode" = drat ] || [ -x "$LRAT_CHECK" ] || continue
      proof="$cnf.canonical.$mode"
      "$CONGRUENCE_TEST" "$cnf" "$proof" "$mode" "canonical-$kind" \
        > "$T/out" && rc=$? || rc=$?
      [ "$rc" -eq 20 ] \
        || { echo "FAIL: canonical Congruence $kind $mode helper (rc=$rc)"; exit 1; }
      if [ "$mode" = drat ]; then
        "$DRAT_TRIM" "$cnf" "$proof" > "$T/check" 2>&1 || true
        grep -q 's VERIFIED' "$T/check" \
          || { echo "FAIL: canonical Congruence $kind DRAT rejected"; \
               cat "$T/check"; exit 1; }
      else
        "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
          || { echo "FAIL: canonical Congruence $kind LRAT rejected"; \
               cat "$T/check"; exit 1; }
        grep -q 'c VERIFIED' "$T/check" \
          || { echo "FAIL: no canonical Congruence $kind LRAT verdict"; \
               cat "$T/check"; exit 1; }
      fi
    done
  done
fi

# ---- the same formulas with BVE in the proof. The generic stress scenario
# BVE at the root, so BVE's resolvent/deletion deltas must compose into a valid
# DRAT + native-LRAT proof: enum2 is refuted by BVE alone (the equivalence-gate
# path derives {b},{¬b}→∅), xor3/php3 mix resolvents with a search-proof tail.
for cnf in "$T/enum2.cnf" "$T/xor3.cnf" "$T/php3.cnf"; do
  for mode in binary ascii; do
    proof="$cnf.bve.$mode.dsr"
    if [ "$mode" = binary ]; then
      TACTIX_STRESS_RULES=BVE "$TACTIX" -q --stress-policy "$cnf" "$proof" > "$T/out" && rc=$? || rc=$?
    else
      TACTIX_STRESS_RULES=BVE "$TACTIX" -q --stress-policy --no-binary "$cnf" "$proof" > "$T/out" \
        && rc=$? || rc=$?
    fi
    [ "$rc" -eq 20 ] || { echo "FAIL: $cnf not UNSAT under BVE (rc=$rc)"; exit 1; }
    grep -q '^s UNSATISFIABLE$' "$T/out" \
      || { echo "FAIL: $cnf (BVE) missing s line"; exit 1; }
    check_dsr "$cnf" "$proof"
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
  proof="$cnf.$mode.dsr"
  extra=""
  [ "$mode" = ascii ] && extra="--no-binary"
  # shellcheck disable=SC2086
  TACTIX_STRESS_RULES=BVE "$TACTIX" --stress-policy --bve-gates=def $extra "$cnf" "$proof" \
    > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] || { echo "FAIL: bvedef not UNSAT (rc=$rc)"; exit 1; }
  grep -qE '^c bve-gates .*[1-9][0-9]* definition' "$T/out" \
    || { echo "FAIL: no definition gate extracted — the case is vacuous"; \
         grep '^c bve' "$T/out"; exit 1; }
  check_dsr "$cnf" "$proof"
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

# ---- BVE one-sided definition core. The four x-side clauses reduce to the
# no-UP enum2 refutation, so the embedded proof has to be lifted lemma by lemma
# to derive x; (not x v c) keeps a nonempty opposite side. The independent
# d/e enum2 tail makes the whole input UNSAT without pre-fixing x. Structural
# score/order selects x first under the BVE stress policy.
printf 'p cnf 6 9\n1 2 3 0\n1 2 -3 0\n1 -2 3 0\n1 -2 -3 0\n-1 4 0\n5 6 0\n5 -6 0\n-5 6 0\n-5 -6 0\n' \
  > "$T/bvedefunit.cnf"
cnf="$T/bvedefunit.cnf"
for mode in binary ascii; do
  proof="$cnf.$mode.dsr"
  extra=""
  [ "$mode" = ascii ] && extra="--no-binary"
  # shellcheck disable=SC2086
  TACTIX_STRESS_RULES=BVE "$TACTIX" --stress-policy --bve-gates=def $extra "$cnf" "$proof" \
    > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] || { echo "FAIL: bvedefunit not UNSAT (rc=$rc)"; exit 1; }
  grep -qE '^c bve-def .* [1-9][0-9]* units$' "$T/out" \
    || { echo "FAIL: no one-sided definition unit extracted"; \
         grep '^c bve' "$T/out"; exit 1; }
  check_dsr "$cnf" "$proof"
done
if [ -x "$LRAT_CHECK" ]; then
  proof="$cnf.lrat"
  TACTIX_STRESS_RULES=BVE "$TACTIX" --stress-policy --bve-gates=def --lrat "$cnf" "$proof" \
    > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] || { echo "FAIL: bvedefunit not UNSAT under --lrat"; exit 1; }
  "$LRAT_CHECK" "$cnf" "$proof" > "$T/check" 2>&1 \
    || { echo "FAIL: native LRAT of one-sided definition rejected"; \
         cat "$T/check"; exit 1; }
  grep -q 'c VERIFIED' "$T/check" \
    || { echo "FAIL: lrat-check gave no verdict on bvedefunit"; \
         cat "$T/check"; exit 1; }
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
    proof="$cnf.fac.$mode.dsr"
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
    check_dsr "$cnf" "$proof"
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

# The public exact-grid surface also admits the equality case m*n=m+n (2x2).
# Drive that opt-in action directly: the shipped canonical schedule remains
# profitability-filtered. Check its SAT projection against the original CNF,
# then its UNSAT proof in both proof formats.
if [ -n "$FACTOR_N1_TEST" ]; then
  printf 'p cnf 4 4\n1 3 0\n1 4 0\n2 3 0\n2 4 0\n' > "$T/factor_zero_sat.cnf"
  "$FACTOR_N1_TEST" "$T/factor_zero_sat.cnf" \
    "$T/factor_zero_sat.drat" drat > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 10 ] || { echo "FAIL: zero-gain Factor SAT witness (rc=$rc)"; exit 1; }
  "$ROOT/tools/fuzz/checkmodel" "$T/factor_zero_sat.cnf" "$T/out" \
    || { echo "FAIL: zero-gain Factor model rejected"; exit 1; }

  printf 'p cnf 6 8\n1 3 0\n1 4 0\n2 3 0\n2 4 0\n5 6 0\n5 -6 0\n-5 6 0\n-5 -6 0\n' \
    > "$T/factor_zero_unsat.cnf"
  for mode in drat lrat; do
    [ "$mode" = drat ] || [ -x "$LRAT_CHECK" ] || continue
    proof="$T/factor_zero_unsat.$mode"
    "$FACTOR_N1_TEST" "$T/factor_zero_unsat.cnf" "$proof" "$mode" \
      > "$T/out" && rc=$? || rc=$?
    [ "$rc" -eq 20 ] || { echo "FAIL: zero-gain Factor UNSAT witness (rc=$rc)"; exit 1; }
    if [ "$mode" = drat ]; then
      "$DRAT_TRIM" "$T/factor_zero_unsat.cnf" "$proof" > "$T/check" 2>&1 || true
      grep -q 's VERIFIED' "$T/check" \
        || { echo "FAIL: zero-gain Factor DRAT rejected"; cat "$T/check"; exit 1; }
    else
      "$LRAT_CHECK" "$T/factor_zero_unsat.cnf" "$proof" > "$T/check" 2>&1 \
        || { echo "FAIL: zero-gain Factor LRAT rejected"; cat "$T/check"; exit 1; }
      grep -q 'c VERIFIED' "$T/check" \
        || { echo "FAIL: zero-gain Factor LRAT missing verdict"; cat "$T/check"; exit 1; }
    fi
  done
fi
# V3 rewrite chain order (corpus 019-022 family): a substituted literal whose
# representative is root-false needs that unit BEFORE the bridging lemma, since
# the unit is what makes the lemma unit. Reached through a legal random
# schedule rather than the default one, so drive that schedule explicitly.
if [ -x "$LRAT_CHECK" ]; then
  printf 'p cnf 7 7\n-4 3 1 0\n-5 4 0\n-6 2 0\n6 -2 0\n7 6 2 0\n-6 -2 0\n-7 0\n' \
    > "$T/substchain.cnf"
  "$TACTIX" -q --random-policy=1 --lrat "$T/substchain.cnf" \
    "$T/substchain.lrat" > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq 20 ] || { echo "FAIL: substchain not UNSAT (rc=$rc)"; exit 1; }
  "$LRAT_CHECK" "$T/substchain.cnf" "$T/substchain.lrat" > "$T/check" 2>&1 || true
  grep -q 'c VERIFIED' "$T/check" \
    || { echo "FAIL: native LRAT of substchain rejected"; cat "$T/check"; exit 1; }
fi

echo "micro_proofs OK"
