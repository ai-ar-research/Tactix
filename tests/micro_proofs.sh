#!/usr/bin/env bash
# Tiny formulas through the real binary and rule fixtures: every DSR proof is
# checked lemma by lemma, every model by checkmodel, and negative controls show
# the reference pipeline rejects invalid proofs. Exits 77 without the checkers.
set -euo pipefail
TACTIX="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
ANALYZE_TEST="${3:-}"
CONGRUENCE_TEST="${4:-}"
RESTART_TEST="${5:-}"
FACTOR_N1_TEST="${6:-}"
ONLINE_INPROCESS_TEST="${7:-}"
ULC_RAT_TEST="${8:-}"
PUBLIC_RANDOM="${9:-}"
for checker in "$DSR_TRIM" "$SR_CHECK"; do
  [ -x "$checker" ] \
    || { echo "native DSR checker pipeline not built — skipping"; exit 77; }
done

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

# check_dsr CNF PROOF: the reference pipeline checks every lemma of PROOF.
check_dsr() {
  local cnf="$1" proof="$2"
  python3 "${TACTIX_PROOF_ORACLE_DIR:-$ROOT/tools/bench}/proof_oracle.py" \
    check-dsr "$cnf" "$proof" --dsr-trim "$DSR_TRIM" \
    --dsr-compress "$DSR_COMPRESS" --srcheck "$SR_CHECK" --every-lemma \
    > "$T/dsr-check.out" 2>&1 \
    || { echo "FAIL: reference DSR pipeline rejected $proof"; \
         cat "$T/dsr-check.out"; return 1; }
}

# reject_dsr CNF PROOF MESSAGE: the reference pipeline must reject PROOF.
reject_dsr() {
  if check_dsr "$1" "$2" > /dev/null 2>&1; then
    echo "FAIL: $3"
    exit 1
  fi
}

# expect_rc RC LABEL COMMAND...: COMMAND, with stdout in $T/out, exits RC.
expect_rc() {
  local want=$1 label=$2 rc
  shift 2
  "$@" > "$T/out" && rc=$? || rc=$?
  [ "$rc" -eq "$want" ] || { echo "FAIL: $label (rc=$rc)"; cat "$T/out"; exit 1; }
}

# refute CNF [ENV=VALUE...]: the CLI reports UNSAT with a checked binary and
# ASCII DSR proof.
refute() {
  local cnf=$1 mode
  shift
  for mode in binary ascii; do
    expect_rc 20 "$cnf not UNSAT" env "$@" "$TACTIX" -q --dsr \
      $([ "$mode" = binary ] || echo --no-binary) "$cnf" "$cnf.$mode.dsr"
    grep -q '^s UNSATISFIABLE$' "$T/out" || { echo "FAIL: $cnf missing s line"; exit 1; }
    check_dsr "$cnf" "$cnf.$mode.dsr"
  done
}

make_cases() {
  # contradictory units
  printf 'p cnf 1 2\n1 0\n-1 0\n' > "$T/units.cnf"
  # classic 2-var full enumeration
  printf 'p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n' > "$T/enum2.cnf"
  # Sparse dependency elaboration: 64 irrelevant root units, then a
  # two-variable contradiction.
  python3 - "$T/sparse-dependencies.cnf" <<'EOF'
import sys

units = 64
with open(sys.argv[1], "w") as output:
    output.write(f"p cnf {units + 2} {units + 4}\n")
    for variable in range(1, units + 1):
        output.write(f"{variable} 0\n")
    left, right = units + 1, units + 2
    output.write(f"{left} {right} 0\n")
    output.write(f"{left} -{right} 0\n")
    output.write(f"-{left} {right} 0\n")
    output.write(f"-{left} -{right} 0\n")
EOF
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
  # A repeated input literal makes the stored clause differ from the file
  # clause, so citing chains need a restatement lemma; one case per width.
  printf 'p cnf 3 4\n-1 -1 0\n-1 2 3 0\n-2 0\n1 1 0\n' > "$T/dupunit.cnf"
  printf 'p cnf 5 6\n5 1 0\n-5 4 0\n5 -1 0\n-4 3 3 0\n-2 0\n-4 -3 0\n' \
    > "$T/dupbinary.cnf"
  printf 'p cnf 4 8\n1 1 2 3 0\n-2 0\n-3 0\n-1 4 0\n-1 -4 0\n2 3 4 0\n-4 2 0\n-4 3 0\n' \
    > "$T/duplarge.cnf"
  # A prior root unit shrinks a later input binary to a unit that needs a
  # deferred restatement lemma, cited by the empty clause.
  printf 'p cnf 2 3\n1 0\n-1 2 0\n-2 0\n' > "$T/rootshrink.cnf"
  # A unit cascade closed during input shrinks a later ternary, and the
  # following units refute during ingestion.
  printf 'p cnf 5 6\n-1 2 0\n-2 3 0\n1 0\n-3 4 5 0\n-4 0\n-5 0\n' \
    > "$T/rootcascade.cnf"
  # A clause satisfied at input stays out of the live solver under a proof;
  # the 3/4 core makes the proof nontrivial.
  printf 'p cnf 4 7\n-1 2 0\n1 0\n2 3 0\n3 4 0\n3 -4 0\n-3 4 0\n-3 -4 0\n' \
    > "$T/root-satisfied.cnf"
  # A later SR witness must survive pure elimination of a normalized input
  # clause (root shortening and repeated literals).
  cat > "$T/sr-input-root.cnf" <<'EOF'
p cnf 9 10
-5 -6 0
-6 5 0
-6 7 0
-7 -5 0
-2 0
-7 5 0
-3 4 2 0
3 -1 0
9 8 0
6 7 0
EOF
  cat > "$T/sr-input-repeat.cnf" <<'EOF'
p cnf 16 15
11 0
5 0
-10 0
7 0
9 8 0
-8 9 0
-14 16 -14 0
-9 -13 0
-12 0
14 15 0
3 0
-2 0
-9 13 0
6 0
4 5 -1 0
EOF
  # Binary deduplication must retire every discarded physical input copy.
  sed 's/p cnf 9 10/p cnf 9 11/; /3 -1 0/a 3 -1 0' \
    "$T/sr-input-root.cnf" > "$T/sr-input-duplicate.cnf"
  # This discarded input duplicate itself shortens to the stored binary.
  sed 's/p cnf 9 10/p cnf 9 11/; /-3 4 2 0/a -3 4 2 0' \
    "$T/sr-input-root.cnf" > "$T/sr-input-normalized-duplicate.cnf"
  # Retained satisfied and tautological input clauses remain SR-safe.
  sed 's/p cnf 9 10/p cnf 9 12/; /-2 0/a -2 3 4 0\n3 -3 4 0' \
    "$T/sr-input-root.cnf" > "$T/sr-input-inert-raw.cnf"
  # Startup collection rewrites a large input clause to (5 6) after initial
  # propagation fixes x1.
  printf 'p cnf 6 7\n-1 5 6 0\n-2 1 0\n2 0\n3 4 0\n3 -4 0\n-3 4 0\n-3 -4 0\n' \
    > "$T/sparsecollect.cnf"
  # A refutation during input must not take a later input clause's id.
  printf 'p cnf 2 4\n1 0\n-1 0\n2 0\n-2 0\n' > "$T/midrefute.cnf"
  # cyclic xor chain, odd total parity: 1^2=1, 2^3=1, 1^3=1 sums to 0=1
  printf 'p cnf 3 6\n1 2 0\n-1 -2 0\n2 3 0\n-2 -3 0\n1 3 0\n-1 -3 0\n' \
    > "$T/xor3.cnf"
  # The StConn d=3,n=3 instance (Lucky15's crossing-sequence refutation).
  cat > "$T/stconn-3x3.cnf" <<'EOF'
p cnf 16 68
-15 -12 0
15 12 0
-10 15 0
10 -15 0
-7 2 0
7 -2 0
-4 -7 0
4 7 0
-16 12 0
16 -12 0
-5 1 0
5 -1 0
-11 -16 -10 0
-13 11 16 10 0
-13 -16 -10 0
13 -11 16 10 0
-13 -11 -10 0
13 11 -16 10 0
-13 -11 -16 0
13 11 16 -10 0
-3 -5 -2 0
-8 3 5 2 0
-8 -5 -2 0
8 -3 5 2 0
-8 -3 -2 0
8 3 -5 2 0
-8 -3 -5 0
8 3 5 -2 0
-9 13 0
9 -13 0
-8 4 0
8 -4 0
6 1 0
-6 -1 0
-14 11 0
14 -11 0
-6 3 0
6 -3 0
-14 -9 0
14 9 0
-10 -2 0
-10 -7 0
-15 -7 0
-15 -2 0
-16 -5 0
-16 -1 0
-12 -5 0
-12 -1 0
-13 -8 0
-11 -3 0
-13 -3 0
-13 -5 0
-13 -2 0
-11 -8 0
-11 -5 0
-11 -2 0
-16 -8 0
-16 -3 0
-16 -2 0
-10 -8 0
-10 -3 0
-10 -5 0
-9 -8 0
-9 -4 0
-13 -4 0
-14 -6 0
-14 -3 0
-11 -6 0
EOF
  # Two signed ternary cardinality groups over x1..x5 need six falsified
  # literals among five variables (Lucky16's BDD refutation).
  python3 - "$T/signed-cardinality.cnf" <<'EOF'
import itertools
import sys

clauses = []
for group in (range(1, 6), range(-1, -6, -1)):
    clauses.extend(itertools.combinations(group, 3))
with open(sys.argv[1], "w") as output:
    output.write(f"p cnf 5 {len(clauses)}\n")
    for clause in clauses:
        output.write(" ".join(map(str, clause)) + " 0\n")
EOF
}

make_cases
for cnf in "$T"/*.cnf; do
  refute "$cnf"
done

# A DSR checker keeps a deleted reason of an implied unit, so -55 stays implied
# and the witnessed unit 55 is invalid: the pipeline must reject it.
python3 - "$T/implied-unit-delete.cnf" <<'EOF'
import sys

clauses = [[variable] for variable in range(1, 40) if variable != 5]
clauses.append([40])
clauses.extend(([-5], [-55, 5], [56, 57], [56, -57],
                [-56, 57], [-56, -57]))
with open(sys.argv[1], "w") as output:
    output.write(f"p cnf 57 {len(clauses)}\n")
    for clause in clauses:
        output.write(" ".join(map(str, clause)) + " 0\n")
EOF
cat > "$T/implied-unit-delete.dsr" <<'EOF'
d -55 5 0
55 55 0
56 0
0
EOF
reject_dsr "$T/implied-unit-delete.cnf" "$T/implied-unit-delete.dsr" \
  "reference pipeline accepted a deleted implied-unit reason"

# The signed-cardinality proof comes from Lucky16; half of it must not check.
cnf="$T/signed-cardinality.cnf"
proof="$T/signed-cardinality-reach.dsr"
expect_rc 20 "signed-cardinality fixture not UNSAT" "$TACTIX" --dsr --no-binary "$cnf" "$proof"
grep -Eq '^c signedcard 1 calls, 1 recognized, 2 groups, [1-9][0-9]* BDD nodes, [1-9][0-9]* proof additions, 0 budget stops, 1 UNSAT, 0 interrupted$' \
  "$T/out" \
  || { echo "FAIL: Lucky16 did not produce the signed-cardinality proof"; \
       cat "$T/out"; exit 1; }
check_dsr "$cnf" "$proof"
lines=$(wc -l < "$proof")
head -n "$((lines / 2))" "$proof" > "$T/signed-cardinality-incomplete.dsr"
reject_dsr "$cnf" "$T/signed-cardinality-incomplete.dsr" \
  "reference pipeline accepted the incomplete signed-cardinality proof"

# A direct SR witness (two-row permutation, unit -1) that forward checking must
# validate although the final empty clause does not use it.
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
check_dsr "$T/direct-sr.cnf" "$T/direct-sr.dsr"

# Unique-literal exact-one completion: one pivot-first RAT line per pair.
if [ -n "$ULC_RAT_TEST" ]; then
  "$ULC_RAT_TEST" "$T/ulc-rat.cnf" "$T/ulc-rat.dsr"
  check_dsr "$T/ulc-rat.cnf" "$T/ulc-rat.dsr"
fi

# structured_sr CNF RULE TRANSITIONS [ARG]: the rule fixture refutes CNF with
# a checked DSR proof after effective SR transitions matching TRANSITIONS.
structured_sr() {
  local cnf=$1 rule=$2 transitions=$3 proof="$1.$2.dsr"
  expect_rc 20 "$rule fixture not UNSAT" "$ONLINE_INPROCESS_TEST" "$cnf" "$proof" dsr "$rule" ${4:+"$4"}
  grep -Eq "^c effective structured SR transitions $transitions\$" "$T/out" \
    || { echo "FAIL: $rule fixture made too few effective transitions"; cat "$T/out"; exit 1; }
  check_dsr "$cnf" "$proof"
}

# Typed SR with the native DSR writer; forward elaboration keeps backward
# trimming from hiding a malformed or ineffective SR line.
if [ -n "$ONLINE_INPROCESS_TEST" ]; then
  structured_sr "$T/direct-sr.cnf" OrbitopalSR '[1-9][0-9]*'

  # A three-literal orbit needs two binary SST cuts before its representative
  # unit is RUP.
  cat > "$T/sst-chain.cnf" <<'EOF'
p cnf 6 9
1 2 3 0
4 5 6 0
4 5 -6 0
4 -5 6 0
4 -5 -6 0
-4 5 6 0
-4 5 -6 0
-4 -5 6 0
-4 -5 -6 0
EOF
  structured_sr "$T/sst-chain.cnf" SstSR '[3-9][0-9]*'

  # No clause of the first component lies within its literal orbits, so the
  # two-pass producer must reach a full orbit cut, the first proof line.
  cat > "$T/fix-and-cut.cnf" <<'EOF'
p cnf 11 39
1 3 0
2 4 0
-3 5 0
-4 6 0
7 8 9 0
-9 -10 11 0
-7 9 -11 0
-7 -9 10 0
-7 -10 11 0
7 -10 11 0
-8 10 11 0
8 10 -11 0
7 -8 -11 0
-8 -9 11 0
-8 9 10 0
7 9 -11 0
9 -10 11 0
7 -8 -9 0
7 8 -10 0
7 -10 -11 0
-7 9 -10 0
-7 -8 10 0
-8 9 -11 0
-9 10 11 0
7 -9 10 0
8 -9 11 0
-9 10 -11 0
-7 8 -10 0
-7 -9 -11 0
-7 8 -11 0
-8 9 -10 0
7 10 -11 0
8 9 -11 0
-9 -10 -11 0
-8 -9 -11 0
-7 -10 -11 0
8 9 -10 0
-7 8 -9 0
-7 8 9 0
EOF
  structured_sr "$T/fix-and-cut.cnf" FixAndCutSR '[1-9][0-9]*' all
  awk 'NR == 1 { if ($1 >= 0 || $2 <= 0 || NF <= 4) exit 1 }' "$T/fix-and-cut.cnf.FixAndCutSR.dsr" \
    || { echo "FAIL: first generated proof action was not an orbit cut"; \
         head -n 1 "$T/fix-and-cut.cnf.FixAndCutSR.dsr"; exit 1; }
fi

# The witnessed unit 1 is invalid ((-1 v 2) reduces to (2) and F & -1 & -2 has
# no propagation). Backward dsr-trim alone accepts it; Trestle rejects it.
printf 'p cnf 5 7\n-1 2 0\n-2 3 0\n-2 -3 0\n1 4 5 0\n1 4 -5 0\n1 -4 5 0\n1 -4 -5 0\n' \
  > "$T/needed-bad-witness.cnf"
printf '1 1 0\n0\n' > "$T/needed-bad-witness.dsr"
reject_dsr "$T/needed-bad-witness.cnf" "$T/needed-bad-witness.dsr" \
  "reference pipeline accepted an invalid witnessed unit"
printf -- '-2 0\n-1 0\n4 0\n0\n' > "$T/needed-bad-witness-honest.dsr"
check_dsr "$T/needed-bad-witness.cnf" "$T/needed-bad-witness-honest.dsr"

# The pinned dsr-trim also rejects a valid, non-RUP witnessed unit whose
# propagation closes the refutation, so Tactix writes the general encoding
# (vacuous RAT on a fresh auxiliary, the witnessed nonunit, then the unit).
printf 'p cnf 8 13\n1 2 0\n1 3 0\n-2 -3 4 0\n-1 4 0\n-1 5 0\n-5 -4 6 0\n-5 -6 0\n1 5 7 0\n1 5 -7 0\n1 7 8 0\n1 7 -8 0\n1 -7 8 0\n1 -7 -8 0\n' \
  > "$T/closing-witness.cnf"
printf '1 1 0\n0\n' > "$T/closing-witness-plain.dsr"
reject_dsr "$T/closing-witness.cnf" "$T/closing-witness-plain.dsr" \
  "the pinned pipeline now accepts a closing witnessed unit"
printf -- '-9 1 0\n9 1 9 1 0\n1 0\nd 9 1 0\nd -9 1 0\n0\n' \
  > "$T/closing-witness-general.dsr"
check_dsr "$T/closing-witness.cnf" "$T/closing-witness-general.dsr"
# In that encoding dsr-trim itself refuses the invalid unit.
printf -- '-6 1 0\n6 1 6 1 0\n1 0\nd 6 1 0\nd -6 1 0\n0\n' \
  > "$T/needed-bad-witness-general.dsr"
reject_dsr "$T/needed-bad-witness.cnf" "$T/needed-bad-witness-general.dsr" \
  "reference pipeline accepted an invalid witnessed unit (general encoding)"

# Trestle rejects an incomplete final hint chain.
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

# Analyze's level-one failed-literal path emits several root units from one
# conflict.
if [ -n "$ANALYZE_TEST" ]; then
  cnf="$T/failed-level-one.cnf"
  printf 'p cnf 5 6\n-1 2 0\n-2 3 0\n-2 4 0\n-3 -4 0\n1 5 0\n1 -5 0\n' \
    > "$cnf"
  proof="$cnf.dsr"
  "$ANALYZE_TEST" "$proof" > "$T/out"
  check_dsr "$cnf" "$proof"
fi

# Restart discharges a conflict of chronological level-zero implications
# before a root rewrite can retire its antecedent.
if [ -n "$RESTART_TEST" ]; then
  cnf="$T/restart-root-conflict.cnf"
  printf 'p cnf 3 3\n-1 0\n-2 0\n1 2 0\n' > "$cnf"
  proof="$cnf.dsr"
  "$RESTART_TEST" "$proof"
  check_dsr "$cnf" "$proof"
fi

# Congruence's semantic surface carries its canonical closure's proof
# obligations; the fixture asserts it applied exactly the requested step.
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
  for kind in v6a gate-unit unary clash literal-clash literal-xor-clash \
              literal-ite-clash canonical-literal-xor-clash canonical-literal-ite-clash; do
    cnf="$T/congruence-${kind#canonical-}.cnf"
    expect_rc 20 "Congruence $kind helper" "$CONGRUENCE_TEST" "$cnf" "$cnf.$kind.dsr" dsr "$kind"
    check_dsr "$cnf" "$cnf.$kind.dsr"
  done
fi

# Factor (BVA) dividers emit vacuous RAT and quotients RAT with one group per
# divider; the duplicate cases delete every copy of a binary and of equal large
# clauses with distinct ids. TACTIX_NO_BVE keeps BVE off the grid.
printf 'p cnf 7 13\n1 3 0\n1 4 0\n1 5 0\n2 3 0\n2 4 0\n2 5 0\n-1 -2 0\n-3 -4 0\n-5 -3 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n' > "$T/factor.cnf"
printf 'p cnf 7 14\n1 3 0\n1 3 0\n1 4 0\n1 5 0\n2 3 0\n2 4 0\n2 5 0\n-1 -2 0\n-3 -4 0\n-5 -3 0\n6 7 0\n6 -7 0\n-6 7 0\n-6 -7 0\n' > "$T/factor_dup.cnf"
printf 'p cnf 6 10\n1 3 4 0\n2 3 4 0\n1 3 4 0\n2 3 4 0\n1 3 4 0\n2 3 4 0\n5 6 0\n5 -6 0\n-5 6 0\n-5 -6 0\n' > "$T/factor_dup_large.cnf"
for cnf in "$T/factor.cnf" "$T/factor_dup.cnf" "$T/factor_dup_large.cnf"; do
  refute "$cnf" TACTIX_NO_BVE=1
done

# The opt-in public exact-grid action also admits the zero-gain 2x2 grid:
# a checked SAT model, then a checked UNSAT proof.
if [ -n "$FACTOR_N1_TEST" ]; then
  printf 'p cnf 4 4\n1 3 0\n1 4 0\n2 3 0\n2 4 0\n' > "$T/factor_zero_sat.cnf"
  expect_rc 10 "zero-gain Factor SAT witness" "$FACTOR_N1_TEST" "$T/factor_zero_sat.cnf" \
    "$T/factor_zero_sat.dsr"
  "$ROOT/tools/fuzz/checkmodel" "$T/factor_zero_sat.cnf" "$T/out" \
    || { echo "FAIL: zero-gain Factor model rejected"; exit 1; }

  printf 'p cnf 6 8\n1 3 0\n1 4 0\n2 3 0\n2 4 0\n5 6 0\n5 -6 0\n-5 6 0\n-5 -6 0\n' \
    > "$T/factor_zero_unsat.cnf"
  expect_rc 20 "zero-gain Factor UNSAT witness" "$FACTOR_N1_TEST" "$T/factor_zero_unsat.cnf" \
    "$T/factor_zero_unsat.dsr"
  check_dsr "$T/factor_zero_unsat.cnf" "$T/factor_zero_unsat.dsr"
fi

# A substituted literal whose representative is root-false needs that unit
# before the bridging lemma; random policy 1 reaches it.
printf 'p cnf 7 7\n-4 3 1 0\n-5 4 0\n-6 2 0\n6 -2 0\n7 6 2 0\n-6 -2 0\n-7 0\n' \
  > "$T/substchain.cnf"
expect_rc 20 "substchain not UNSAT" "$PUBLIC_RANDOM" --random-policy=1 --dsr --no-binary \
  "$T/substchain.cnf" "$T/substchain.dsr"
check_dsr "$T/substchain.cnf" "$T/substchain.dsr"

# Structured advice accepts unequal duplicate counts (checked SAT and UNSAT).
if [ -n "$ONLINE_INPROCESS_TEST" ]; then
  corpus="$ROOT/tools/fuzz/corpus/regressions/sr-unequal-duplicate-handoff"
  for case in min case; do
    for arm in production diagnostic; do
      cnf="$corpus/$case.cnf"
      proof="$T/unequal-$case-$arm.dsr"
      command=("$TACTIX" --dsr "$cnf" "$proof")
      [ "$arm" = production ] || command=("$ONLINE_INPROCESS_TEST" "$cnf" "$proof" dsr CombinedSR all)
      if [ "$case" = min ]; then
        expect_rc 10 "unequal duplicate SAT" "${command[@]}"
        "$ROOT/tools/fuzz/checkmodel" "$cnf" "$T/out"
      else
        expect_rc 20 "unequal duplicate UNSAT" "${command[@]}"
        check_dsr "$cnf" "$proof"
      fi
    done
  done
fi

# Combined startup keeps the PureOnly predecessor before individual SR
# sub-strategies (a pure variable; checked model or proof).
if [ -n "$ONLINE_INPROCESS_TEST" ]; then
  for vertices in 4 5; do
    for sign in 1 -1; do
      cnf="$T/combined-pure-$vertices-$sign.cnf"
      python3 - "$cnf" "$vertices" "$sign" <<'PY'
import sys
path, vertices, sign = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
lit = lambda v, c: sign * (3 * v + c + 1)
clauses = []
for v in range(vertices):
    clauses.append([lit(v, c) for c in range(3)])
    clauses.extend([-lit(v, a), -lit(v, b)] for a in range(3) for b in range(a + 1, 3))
for a in range(vertices):
    for b in range(a + 1, vertices):
        if a or b == 1:
            clauses.extend([-lit(a, c), -lit(b, c)] for c in range(3))
clauses.append([sign * (3 * vertices + 1), lit(0, 0)])
with open(path, 'w') as out:
    out.write(f'p cnf {3 * vertices + 1} {len(clauses)}\n')
    for clause in clauses:
        out.write(' '.join(map(str, clause)) + ' 0\n')
PY
      expect_rc $((vertices == 4 ? 10 : 20)) "CombinedSR pure $vertices" \
        "$ONLINE_INPROCESS_TEST" "$cnf" "$T/combined-pure.dsr" dsr CombinedSR all
      grep -q 'c structured step 94 .*progress 1' "$T/out" \
        || { echo "FAIL: CombinedSR omitted its PureOnly transition"; exit 1; }
      if [ "$vertices" = 4 ]; then
        "$ROOT/tools/fuzz/checkmodel" "$cnf" "$T/out"
      else
        check_dsr "$cnf" "$T/combined-pure.dsr"
      fi
    done
  done
fi

# Product-axis completion moves coupled auxiliary variables, tracing leaves the
# certificate unchanged, and SAT/UNSAT results check.
if [ -n "$ONLINE_INPROCESS_TEST" ]; then
  for unsat in 0 1; do
    for sign in 1 -1; do
      cnf="$T/product-coupled-$unsat-$sign.cnf"
      python3 - "$cnf" "$unsat" "$sign" <<'PYFIXTURE'
import sys
path, unsat, sign = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
x = lambda r, c: sign * (3*r+c+1)
y = lambda c: sign * (10+c)
clauses = [[x(r,c) for c in range(3)] for r in range(3)]
clauses += [[-x(r,c), y(c)] for r in range(3) for c in range(3)]
clauses += [[-x(a,c), -x(b,c)] for c in range(3) for a in range(3) for b in range(a+1,3)]
if unsat:
    # An independent core: a clause over y could let a row producer solve first.
    clauses += [[a*13, b*14] for a in (1, -1) for b in (1, -1)]
with open(path, 'w') as out:
    out.write(f'p cnf {14 if unsat else 12} {len(clauses)}\n')
    for clause in clauses:
        out.write(' '.join(map(str, clause)) + ' 0\n')
PYFIXTURE
      for trace in 0 1; do
        proof="$T/product-coupled-$trace.dsr"
        TACTIX_SR_TRACE="$trace" "$ONLINE_INPROCESS_TEST" "$cnf" "$proof" dsr OrbitopalProductSR all \
          > "$T/product-out-$trace" 2> "$T/product-trace-$trace" && rc=$? || rc=$?
        grep -Eq 'c structured totals applications [1-9][0-9]* applied [1-9]' "$T/product-out-$trace" \
          || { echo "FAIL: coupled product made no effective SR application"; exit 1; }
        if [ "$unsat" = 1 ]; then
          [ "$rc" -eq 20 ] || { echo "FAIL: coupled product UNSAT (rc=$rc)"; exit 1; }
        else
          [ "$rc" -eq 10 ] || { echo "FAIL: coupled product SAT (rc=$rc)"; exit 1; }
          "$ROOT/tools/fuzz/checkmodel" "$cnf" "$T/product-out-$trace"
        fi
      done
      grep -Eq 'product_axis_check.*pure[[:space:]]0' "$T/product-trace-1" \
        || { echo "FAIL: product control did not require permutation completion"; exit 1; }
      grep -Eq 'product_axes_completed.*row_valid[[:space:]]1.*column_valid[[:space:]]1' "$T/product-trace-1" \
        || { echo "FAIL: product control did not complete both axes"; exit 1; }
      cmp "$T/product-coupled-0.dsr" "$T/product-coupled-1.dsr" \
        || { echo "FAIL: product tracing changed the certificate"; exit 1; }
      if [ "$unsat" = 1 ]; then
        check_dsr "$cnf" "$T/product-coupled-1.dsr"
      fi
    done
  done
fi

# Fuzz regression: an SCC image becomes unit only after its inactive source
# endpoint is replaced by a representative fixed after Tarjan froze the map.
corpus="$ROOT/tools/fuzz/corpus/regressions/034-substitute-root-fixed-binary-image-unit/case.cnf"
proof="$T/substitute-root-fixed-binary-image-unit.dsr"
expect_rc 20 "Substitute root-fixed binary image" \
  "$PUBLIC_RANDOM" --random-policy=296716882 --dsr "$corpus" "$proof"
check_dsr "$corpus" "$proof"

# Fuzz regression: a growth-declined BVE candidate keeps its unit resolvents,
# so the round's schedule is stale; the round ends and the model checks.
corpus="$ROOT/tools/fuzz/corpus/regressions/035-bve-round-stale-schedule-fixed-pivot/case.cnf"
expect_rc 10 "BVE round stale-schedule fixed pivot" "$PUBLIC_RANDOM" --random-policy=296725965 "$corpus"
"$ROOT/tools/fuzz/checkmodel" "$corpus" "$T/out" \
  || { echo "FAIL: BVE round stale-schedule model rejected"; exit 1; }

echo "micro_proofs OK"
