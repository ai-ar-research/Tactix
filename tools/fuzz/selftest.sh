#!/usr/bin/env bash
# Harness validation (plan §8.2, phase-0 exit): the pipeline must run clean
# against Kissat and CaDiCaL as stand-in solvers, and must flag each
# deliberately-broken wrapper via the right oracle.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
python3 "$HERE/controls/artifact_isolation_controls.py"
KISSAT="$ROOT/tools/solvers/bin/kissat"
CADICAL="$ROOT/tools/solvers/bin/cadical"
[ -x "$KISSAT" ] && [ -x "$CADICAL" ] || "$ROOT/tools/solvers/setup.sh"
# TACTIX_FUZZ_CHECKERS selects a frozen core-checker collection (see
# slurm.sh --checkers); fuzz.py verifies it against its SHA256SUMS.
CHECKERS="${TACTIX_FUZZ_CHECKERS:-$ROOT/tools/checkers/bin}"
if [ -z "${TACTIX_FUZZ_CHECKERS:-}" ]; then
    [ -x "$CHECKERS/drat-trim" ] || "$ROOT/tools/checkers/setup.sh"
    [ -x "$CHECKERS/cake_lpr" ] || "$ROOT/tools/checkers/setup.sh"
    [ -x "$CHECKERS/dsr-trim" ] || "$ROOT/tools/checkers/setup.sh"
    [ -x "$CHECKERS/srcheck" ] || "$ROOT/tools/checkers/setup.sh"
fi
python3 "$HERE/fuzz.py" checkers
python3 "$HERE/controls/checker_collection_controls.py"
python3 "$HERE/controls/controller_coverage_controls.py"
python3 "$ROOT/tests/test_forced_ttf_fuzz.py"

# Fixed native/conversion dispatch, invalid-empty-lemma, strict status and
# failure-artifact retention controls use the actual acceptance checkers.
python3 "$ROOT/tests/test_fuzz_proof_oracles.py"
# Every-lemma audits: forward drat-trim (witness-free) and forward dsr-trim
# (witnessed) reject unused invalid lemmas the backward routes accept, Trestle
# rejects the retained historical mis-elaborations, and the configured
# checkers accept the valid F2/F2w proofs.
python3 "$HERE/controls/every_lemma_controls.py"

TMP="$(mktemp -d)"
cleanup_selftest() {
    status=$?
    if [ "$status" -eq 0 ]; then
        rm -rf "$TMP"
    else
        echo "retained failed-selftest artifacts: $TMP"
    fi
}
trap cleanup_selftest EXIT
FUZZ="python3 $HERE/fuzz.py"

expect_clean () { # name solver oracle n
    echo "== $1: expect 0 failures =="
    $FUZZ run --solver "$2" --oracle "$3" --n "$4" --lrat \
        --outdir "$TMP/$1" --no-shrink \
        || { echo "SELFTEST FAILED: $1 reported failures"; exit 1; }
}

expect_flagged () { # name solver oracle n kind
    echo "== $1: expect >=1 '$5' failure =="
    $FUZZ run --solver "$2" --oracle "$3" --n "$4" \
        --outdir "$TMP/$1" --no-shrink --max-failures 3 > "$TMP/$1.log" 2>&1 \
        && { echo "SELFTEST FAILED: $1 not flagged"; cat "$TMP/$1.log"; exit 1; }
    grep -q "'$5'" "$TMP/$1.log" \
        || { echo "SELFTEST FAILED: $1 flagged, but not as '$5'"; cat "$TMP/$1.log"; exit 1; }
    echo "flagged as '$5' — OK"
}

expect_clean kissat  "$KISSAT"  "$CADICAL" 150
expect_clean cadical "$CADICAL" "$KISSAT"  100

expect_flagged lie-unsat-as-sat "$HERE/broken/lie-unsat-as-sat.sh" "$KISSAT" 60 model
expect_flagged negate-model    "$HERE/broken/negate-model.sh"     "$KISSAT" 60 model
expect_flagged truncate-proof  "$HERE/broken/truncate-proof.sh"   "$KISSAT" 60 proof

echo "== ddmin: planted 2-clause core buried in junk must shrink out =="
# contradictory units on var 1 + 400 satisfiable random 3-clauses on vars
# 2..60; kissat refutes it instantly, the lying wrapper flags 'model', and
# a working shrinker must recover (nearly) just the planted core.
python3 - "$TMP/planted.cnf" <<'EOF'
import random, sys
sys.path.insert(0, __import__('os').path.dirname(sys.argv[0]) or '.')
rng = random.Random(1)
clauses = [[1], [-1]]
for _ in range(400):
    vs = rng.sample(range(2, 61), 3)
    clauses.append([v if rng.random() < 0.7 else -v for v in vs])
with open(sys.argv[1], "w") as f:
    f.write(f"p cnf 60 {len(clauses)}\n")
    for c in clauses:
        f.write(" ".join(map(str, c)) + " 0\n")
EOF
"$HERE/ddmin" "$TMP/planted.cnf" --out "$TMP/planted-min.cnf" -- \
    python3 "$HERE/fuzz.py" check-one \
    --solver "$HERE/broken/lie-unsat-as-sat.sh" --fail-kind model \
    --timeout 10
MINCLAUSES=$(awk '/^p cnf/ {print $4}' "$TMP/planted-min.cnf")
echo "shrunk to $MINCLAUSES clauses"
[ "$MINCLAUSES" -le 5 ] || { echo "SELFTEST FAILED: shrink ineffective"; exit 1; }

echo "== duplicate input literals: the CONVERSION cross-check must stand down, =="
echo "== and the NATIVE-LRAT oracle must still fire                          =="
# drat-trim deletes duplicate literals as it parses the CNF, lrat-check does
# not, and `drat-trim -L` cites input clauses by id. So on this 5-clause
# formula the converted LRAT is rejected for every solver, kissat's and
# cadical's own proofs included — a checker disagreement, not a solver defect,
# so the cross-check must skip visibly and counted rather than blame the
# solver. Evidence:
# benchmarks/reports/2026-08-05-drat-trim-conversion-crosscheck.md.
# The native-LRAT oracle reads ids the emitter here wrote and must keep
# checking these cases: it is the oracle covering the frontend restatement.
printf 'p cnf 3 5\n1 1 0\n-1 2 0\n-1 3 0\n-2 -3 0\n2 3 0\n' > "$TMP/dup.cnf"
# (a) the conversion cross-check stands down, and says so
$FUZZ check-one --solver "$KISSAT" --timeout 10 --lrat "$TMP/dup.cnf" \
    > "$TMP/dup-conv.log" 2>&1 || true
grep -q "skipped=\[('lrat-convert'" "$TMP/dup-conv.log" \
    || { echo "SELFTEST FAILED: conversion cross-check not skipped on a"; \
         echo "  duplicate-literal CNF"; cat "$TMP/dup-conv.log"; exit 1; }
grep -q "failures=\[\]" "$TMP/dup-conv.log" \
    || { echo "SELFTEST FAILED: duplicate-literal CNF blamed on the solver"; \
         cat "$TMP/dup-conv.log"; exit 1; }
# (b) the raw pipeline really does reject here -- otherwise (a) is vacuous
"$KISSAT" -q "$TMP/dup.cnf" "$TMP/dup.drat" > /dev/null 2>&1 || true
"$CHECKERS/drat-trim" "$TMP/dup.cnf" "$TMP/dup.drat" \
    -L "$TMP/dup.lrat" > /dev/null 2>&1 || true
"$CHECKERS/lrat-check" "$TMP/dup.cnf" "$TMP/dup.lrat" \
    > "$TMP/dup-raw.log" 2>&1 || true
grep -q "NOT VERIFIED" "$TMP/dup-raw.log" \
    || { echo "SELFTEST FAILED: the drat-trim -L / lrat-check disagreement is"; \
         echo "  GONE (checkers upgraded?) -- re-enable the cross-check"; \
         cat "$TMP/dup-raw.log"; exit 1; }
echo "conversion cross-check skipped, and the disagreement it dodges is real — OK"
# (c) the native-LRAT oracle still runs and passes on the same CNF
# TACTIX_FUZZ_SELFTEST_TACTIX names the pinned candidate build to use here.
TACTIX="${TACTIX_FUZZ_SELFTEST_TACTIX:-$ROOT/build/debug/tactix}"
if [ ! -x "$TACTIX" ]; then
    echo "native-LRAT control NOT RUN: no executable $TACTIX"
else
    $FUZZ check-one --solver "$TACTIX --lrat" --timeout 10 "$TMP/dup.cnf" \
        > "$TMP/dup-native.log" 2>&1 || true
    grep -q "failures=\[\]" "$TMP/dup-native.log" \
        || { echo "SELFTEST FAILED: native-LRAT oracle rejects a"; \
             echo "  duplicate-literal CNF"; cat "$TMP/dup-native.log"; exit 1; }
    grep -q "skipped=" "$TMP/dup-native.log" \
        && { echo "SELFTEST FAILED: native-LRAT oracle stood down; it must"; \
             echo "  NOT be gated by the duplicate-literal condition"; \
             cat "$TMP/dup-native.log"; exit 1; }
    echo "native-LRAT oracle still fires and passes — OK"
fi

echo "SELFTEST OK"
