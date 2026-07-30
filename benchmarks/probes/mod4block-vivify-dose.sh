#!/usr/bin/env bash
# LOCATED DIVERGENCE (2026-07-29): on mod4block kissat vivifies 47,526 clauses
# (38% of checks) and we vivify 46 — ~1000x — while decisions/conflict (3.23 vs
# 3.16), props/conflict (27.7 vs 38.2) and conflicts/restart (24.1 vs 29) are all
# at parity. Our inprocessing budget there goes to Strengthen (3.8 s, 29.5% of
# wall) and SubsumeForward (1.6 s); Vivify gets 104 ms. Both solvers nominally
# run vivify at 100 per-mille (kissat options.h:161 vivifyeffort=100, our
# EFFORT_SPECS), so the rate is not the difference — which is what makes this
# worth measuring rather than assuming.
# Arms sweep OUR vivify dose under the GAP CONDITION (BCE off, where we need
# 10.08M conflicts vs kissat's 715k). If vivification rises and conflicts fall
# toward kissat's, the lever is Vivify dosing (policy). If checks rise but
# vivifications do not, it is Vivify's candidate selection or scope, not dosing.
set -u
R=/home/hwu/SAT-solving
f=$(ls $R/benchmarks/cache/tierB/mod4block*.cnf.xz | head -1)
cnf=$TMPDIR/m.cnf
xz -dc "$f" > "$cnf"
rate="$1"; bce="$2"
[ "$bce" = off ] && export TACTIX_SLOT_DISABLE=BCE
echo "=== vivify=$rate permille, BCE=$bce ==="
TACTIX_SLOT_PERMILLE=Vivify:$rate /usr/bin/time -f "wall %e" timeout 900 \
  $R/build/pin-viv/tactix "$cnf" 2>&1 |
  grep -E "^s |wall |^c conflicts|^c vivify|^c strengthen|^c subsume|^c bce |^c timing"
rm -f "$cnf"
