#!/usr/bin/env bash
# Tiered vivification (kissat's vivifytier1/2/3 = 3:3:1 over glue bands cut at the
# dynamic k.tier1/k.tier2) vs the single untargeted redundant pass we shipped.
# Hypothesis: the 10x vivify YIELD deficit (2.7-4.0% vs kissat's 38%) is WHICH
# clauses we offer, not how many. CONFIRM = yield rises toward kissat's on
# mod4block; KILL = yield flat, meaning the deficit is in vivify's inner
# algorithm (deduce/subsume) rather than candidate selection.
# Verify the pinned binary against a known baseline before trusting arms off it.
set -u
R=/home/hwu/SAT-solving
inst="$1"; arm="$2"; bce="$3"
f=$(ls $R/benchmarks/cache/tier[BC]/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" > "$cnf"
[ "$bce" = off ] && export TACTIX_SLOT_DISABLE=BCE
[ "$arm" = untiered ] && export TACTIX_VIVIFY_UNTIERED=1
echo "=== $inst arm=$arm BCE=$bce ==="
/usr/bin/time -f "wall %e" timeout 900 $R/build/pin-tier2/tactix "$cnf" 2>&1 |
  grep -E "^s |wall |^c conflicts|^c vivify|^c learnedsize|^c reduction|^c timing      [0-9]|^c timing        Vivify"
rm -f "$cnf"
