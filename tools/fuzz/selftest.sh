#!/usr/bin/env bash
# Harness validation (plan §8.2, phase-0 exit): the pipeline must run clean
# against Kissat and CaDiCaL as stand-in solvers, and must flag each
# deliberately-broken wrapper via the right oracle.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
KISSAT="$ROOT/tools/solvers/bin/kissat"
CADICAL="$ROOT/tools/solvers/bin/cadical"
[ -x "$KISSAT" ] && [ -x "$CADICAL" ] || "$ROOT/tools/solvers/setup.sh"
[ -x "$ROOT/tools/checkers/bin/drat-trim" ] || "$ROOT/tools/checkers/setup.sh"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
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

echo "SELFTEST OK"
