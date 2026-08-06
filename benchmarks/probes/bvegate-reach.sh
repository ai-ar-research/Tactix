#!/usr/bin/env bash
# SP-BVE-GATE-1 reachability probe: prove at the SOURCE that ITE/XOR gate
# extraction FIRES, and how many resolvent pairs it removes from the growth
# bound's input. A green fuzz/A-B leg over zero extracted gates is vacuous
# ([[ab-arm-identical-artefacts]]), so this runs FIRST.
#
#   bvegate-reach.sh <tactix-binary> [n_fuzzsat]
#
# Two populations:
#   (1) FuzzSAT-style Tseitin circuits from the fuzz generator — they emit
#       AND/OR/XOR gates, so an arity-2 XOR gate is present by construction.
#       Driven with --bve-stress-policy so BVE certainly runs.
#   (2) cached Tier B instances under the SHIPPED DefaultPolicy — the arm the
#       A/B will actually score.
# Prints one `c bve-gates` roll-up per population.
set -euo pipefail
TACTIX="$1"
N="${2:-200}"
ROOT="$(git rev-parse --show-toplevel)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

roll() { # stdin: many `c bve-gates` lines → one summed line
  awk '/^c bve-gates/ {e+=$3; a+=$5; i+=$7; x+=$9; s+=$11; n++}
       END {printf "  TOTAL over %d runs: equiv=%d andor=%d ITE=%d XOR=%d pairs_saved=%d\n",
                   n, e, a, i, x, s}'
}

echo "== population 1: $N FuzzSAT circuits, --bve-stress-policy =="
ROOT="$ROOT" python3 - "$T" "$N" <<'EOF'
import sys, os
sys.path.insert(0, os.path.join(os.environ["ROOT"], "tools", "fuzz"))
import generators
out, n = sys.argv[1], int(sys.argv[2])
# same (seed, index) discipline as fuzz.py's one_case: seed = base + i
for i in range(n):
    name, nv, cls = generators.generate(20260805 + 1000 + i, 1000 + i)
    generators.write_dimacs(os.path.join(out, "f%04d.cnf" % i), nv, cls)
EOF
for f in "$T"/f*.cnf; do
  timeout 20 "$TACTIX" --bve-stress-policy "$f" || true
done | grep '^c bve-gates' | tee "$T/p1.txt" | tail -3
roll < "$T/p1.txt"

echo "== population 2: cached Tier B instances, shipped DefaultPolicy =="
CACHE="$ROOT/benchmarks/cache/tierB"
i=0
for xz in "$CACHE"/*.cnf.xz; do
  i=$((i + 1)); [ "$i" -le 12 ] || break
  b="$(basename "$xz" .xz)"
  xz -dc "$xz" > "$T/$b"
  echo "-- $b"
  timeout 120 "$TACTIX" "$T/$b" | grep -E '^c (bve|bve-gates)' || true
  rm -f "$T/$b"
done | tee "$T/p2.txt"
grep '^c bve-gates' "$T/p2.txt" | roll
