#!/usr/bin/env bash
# L20 test for R-d's three Tier C losers (j3037, jgiraldezlevy, urq45).
#
# THE QUESTION: is their regression under retirement REAL DAMAGE, or are they
# perturbation-FRAGILE — collapsing under any substantial change, the way
# ContextModel does (stable under replication, ~20x under an unrelated
# mechanism)?  If fragile, an admission gate fitted to them fits NOISE.
#
# Design: per instance, two replicates of the base (does it reproduce?), the
# retire arm (the observed regression), and THREE UNRELATED perturbations that
# have nothing to do with retirement -- a walk-dose change, a bump-rate change,
# and chronological backtracking off.  A fragile instance moves a lot for ALL of
# them; a genuinely damaged one moves only for retire.
# Manifest: <instance-stem>|<arm>
set -u
R=/home/hwu/SAT-solving
IFS='|' read -r stem arm <<<"$1"
f=$(ls "$R"/benchmarks/cache/tierC/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 0; }
w=$TMPDIR/fr-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" > "$w/i.cnf"
case "$arm" in
  base1|base2) export TACTIX_RETIRE=0 ;;
  retire)      export TACTIX_RETIRE=1 ;;
  walk)        export TACTIX_RETIRE=0 TACTIX_WALK_PERMILLE=400 ;;
  bump)        export TACTIX_RETIRE=0 TACTIX_BUMP_REASONS_RATE=30 ;;
  chrono)      export TACTIX_RETIRE=0 TACTIX_CHRONO_LEVELS=0 ;;
esac
printf "%-14s %-8s " "$stem" "$arm"
/usr/bin/time -f "wall %e s" timeout 1200 "$R/build/pin-ship-rd/tactix" "$w/i.cnf" 2>&1 \
  | grep -E "^c conflicts|^c retired|^s |wall " | tr "\n" " "; echo
rm -rf "$w"
