#!/usr/bin/env bash
# SCHED-H1 · does the abort path fire at FUZZ scale, not only on tier instances?
# Task #10's abort paths passed 4 x 10,000 fuzz cases while inert precisely
# because the harness's CNFs are far smaller than one 256-poll window. This
# generates the harness's own cases and counts `c c1c2stop` over them, so the
# green fuzz legs can be shown non-vacuous rather than assumed to be.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
N=${1:-400}
BIN=$R/build/pin-g2h1/tactix
w=${TMPDIR:-/tmp}/h1reach-$$; mkdir -p "$w"
python3 - "$w" "$N" <<'PY'
import sys, os
sys.path.insert(0, os.path.join(os.environ["R"], "tools", "fuzz"))
import generators
out, n = sys.argv[1], int(sys.argv[2])
for i in range(n):
    _name, nv, cls = generators.generate(20260805 + i, i)
    generators.write_dimacs(os.path.join(out, "c%05d.cnf" % i), nv, cls)
PY
c1=0; c2=0; fires=0; cases=0
for f in "$w"/*.cnf; do
  o=$(TACTIX_STOP_FUZZ=97 timeout 10 "$BIN" --random-policy "$f" 2>/dev/null | \
      grep -E "^c c1c2stop|^c interrupt")
  a=$(echo "$o" | sed -n 's/^c c1c2stop *\([0-9]*\) C1.*/\1/p')
  b=$(echo "$o" | sed -n 's/.*rounds, \([0-9]*\) C2 rounds interrupted/\1/p')
  s=$(echo "$o" | sed -n 's/.*(\([0-9]*\) stress fires)/\1/p')
  c1=$((c1 + ${a:-0})); c2=$((c2 + ${b:-0})); fires=$((fires + ${s:-0})); cases=$((cases + 1))
done
echo "=== h1 fuzz-scale reach: $cases cases, seed 20260805, TACTIX_STOP_FUZZ=97"
echo "c1_interrupts=$c1 c2_interrupts=$c2 stop_fuzz_fires_total=$fires"
rm -rf "$w"
