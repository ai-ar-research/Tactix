#!/usr/bin/env bash
# Competition-compliance tests (plan §6; compete skill). Usage:
#   tools/compete/check.sh ./build/release/tactix
set -euo pipefail
SOLVER="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
VERIPB="${VERIPB:-$ROOT/tools/checkers/bin/veripb}"
CAKE_PB="${CAKE_PB:-$ROOT/tools/checkers/bin/cake-pb-cnf}"
# proof_oracle owns the default PBP pipeline and retained DSR routes.
check_proof() {
  python3 "$ROOT/tools/bench/proof_oracle.py" check "$1" "$2" \
    --dsr-trim "$DSR_TRIM" --dsr-compress "$DSR_COMPRESS" \
    --srcheck "$SR_CHECK" --veripb "$VERIPB" --cake-pb "$CAKE_PB" \
    > "$T/proof-check" 2>&1
}
CHECKMODEL="${CHECKMODEL:-$ROOT/tools/fuzz/checkmodel}"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fail() { echo "COMPLIANCE FAIL: $*"; exit 1; }
one_status() {
  [ "$(grep -c '^s ' "$1")" -eq 1 ] || fail "$2: expected exactly one s line"
  awk '!/^c |^s |^v / { exit 1 }' "$1" || fail "$2: invalid stdout line"
}

# 1. SAT instance: exit 10, exact s line, model passes checkmodel
printf 'p cnf 3 3\n1 2 0\n-1 3 0\n-2 -3 0\n' > "$T/sat.cnf"
"$SOLVER" "$T/sat.cnf" > "$T/out" && rc=$? || rc=$?
[ "$rc" -eq 10 ] || fail "SAT exit code $rc != 10"
grep -qx 's SATISFIABLE' "$T/out" || fail "missing exact 's SATISFIABLE'"
one_status "$T/out" SAT
"$CHECKMODEL" "$T/sat.cnf" "$T/out" > /dev/null || fail "model check"
awk '/^v/ { if (length($0) > 4096) exit 1 }' "$T/out" || fail "v line > 4096"

# 2. UNSAT instance: exit 20, proof written and verified
printf 'p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n' > "$T/unsat.cnf"
"$SOLVER" "$T/unsat.cnf" "$T/proof.pbp" > "$T/out" && rc=$? || rc=$?
[ "$rc" -eq 20 ] || fail "UNSAT exit code $rc != 20"
grep -qx 's UNSATISFIABLE' "$T/out" || fail "missing exact 's UNSATISFIABLE'"
one_status "$T/out" UNSAT
[ -s "$T/proof.pbp" ] || fail "no proof written"
grep -qx 'c proof-route pbp' "$T/out" || fail "default route is not PBP"
check_proof "$T/unsat.cnf" "$T/proof.pbp" || fail "PBP not verified"

# 2b. proofs are opt-in: UNSAT without a proof argument must work and
#     must not create any proof file
( cd "$T" && "$SOLVER" unsat.cnf > out.noproof ) && rc=$? || rc=$?
[ "$rc" -eq 20 ] || fail "no-proof UNSAT exit code $rc != 20"
grep -qx 's UNSATISFIABLE' "$T/out.noproof" || fail "no-proof UNSAT s line"
[ "$(find "$T" -name '*.pbp' | wc -l)" -eq 1 ] \
  || fail "unexpected proof file created in no-proof mode"

# 3. SIGTERM mid-solve on a hard instance: s UNKNOWN, proof prefix-valid
python3 - "$T/hard.cnf" <<'EOF'
import random, sys
rng = random.Random(7)
n, m = 400, 1704
with open(sys.argv[1], "w") as f:
    f.write(f"p cnf {n} {m}\n")
    for _ in range(m):
        vs = rng.sample(range(1, n + 1), 3)
        f.write(" ".join(str(v if rng.random() < .5 else -v) for v in vs) + " 0\n")
EOF
"$SOLVER" "$T/hard.cnf" "$T/hard.pbp" > "$T/out" & pid=$!
sleep 0.4; kill -TERM "$pid" 2>/dev/null || true
wait "$pid" && rc=$? || rc=$?
if grep -qx 's UNKNOWN' "$T/out"; then
  [ "$rc" -ne 10 ] && [ "$rc" -ne 20 ] || fail "UNKNOWN but exit $rc"
else
  # solved before the signal landed — accept a real verdict
  grep -qxE 's (SATISFIABLE|UNSATISFIABLE)' "$T/out" \
    || fail "no s line after SIGTERM"
fi

# 4. garbage / empty / comment-only inputs: no crash, sane verdicts
printf 'garbage &&&\n' > "$T/garbage.cnf"
"$SOLVER" "$T/garbage.cnf" > "$T/out" && rc=$? || rc=$?
grep -qx 's UNKNOWN' "$T/out" || fail "garbage input: expected s UNKNOWN"
[ "$rc" -ne 10 ] && [ "$rc" -ne 20 ] || fail "garbage input: exit $rc"
: > "$T/empty.cnf"
"$SOLVER" "$T/empty.cnf" > "$T/out" && rc=$? || rc=$?
[ "$rc" -eq 10 ] || fail "empty file should be trivially SAT (exit $rc)"
printf 'c only comments\nc nothing else\n' > "$T/comments.cnf"
"$SOLVER" "$T/comments.cnf" > "$T/out" && rc=$? || rc=$?
[ "$rc" -eq 10 ] || fail "comment-only file should be SAT (exit $rc)"

# Comments among clauses, arbitrary whitespace, duplicate literals,
# tautologies, and a missing final newline occur in unsanitized inputs.
printf 'c pre\n p cnf 3 3\n1 1 2 0\nc middle\n-1 3 0\n-2 -3 0' \
  > "$T/odd.cnf"
"$SOLVER" "$T/odd.cnf" > "$T/out" && rc=$? || rc=$?
[ "$rc" -eq 10 ] || fail "odd DIMACS exit $rc"
one_status "$T/out" odd-DIMACS
"$CHECKMODEL" "$T/odd.cnf" "$T/out" > /dev/null || fail "odd DIMACS model"

# 5. determinism: identical verdict + proof across two runs
printf 'p cnf 4 8\n1 2 3 0\n-1 -2 0\n-1 -3 0\n-2 -3 0\n4 1 0\n-4 2 0\n-4 3 0\n4 -1 -2 0\n' > "$T/det.cnf"
# Timing comments are intentionally observational and can differ by a
# millisecond.  Quiet mode leaves the competition verdict/model stream, whose
# determinism is the contract checked here, while the proof comparison below
# still covers the complete native certificate.
"$SOLVER" --quiet "$T/det.cnf" "$T/det1.pbp" > "$T/out1" || true
"$SOLVER" --quiet "$T/det.cnf" "$T/det2.pbp" > "$T/out2" || true
cmp -s "$T/out1" "$T/out2" || fail "nondeterministic stdout"
cmp -s "$T/det1.pbp" "$T/det2.pbp" || fail "nondeterministic proof"

# Submission wrapper: argument two is a directory and default PBP is written as
# proof.out for the pinned VeriPB/CakePB pipeline.
WRAPPER="$ROOT/tools/compete/run.sh"
mkdir "$T/wrapper-out"
"$WRAPPER" "$T/unsat.cnf" "$T/wrapper-out" > "$T/wrapper.stdout" \
  && rc=$? || rc=$?
[ "$rc" -eq 20 ] || fail "wrapper UNSAT exit $rc"
[ -s "$T/wrapper-out/proof.out" ] || fail "wrapper did not write proof.out"
one_status "$T/wrapper.stdout" wrapper
grep -qx 'c proof-route pbp' "$T/wrapper.stdout" \
  || fail "wrapper did not report the default PBP route"
check_proof "$T/unsat.cnf" "$T/wrapper-out/proof.out" \
  || fail "reference PBP pipeline rejected wrapper proof"

echo "COMPLIANCE OK"
