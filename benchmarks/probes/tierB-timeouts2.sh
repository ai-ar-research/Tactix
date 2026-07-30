#!/usr/bin/env bash
# WAVE 2 of the Tier B timeout diagnosis (see tierB-timeouts.sh for wave 1).
# Wave 1 established the shared SIGNATURE: every Tier B instance on which we
# lose >2.3x is binary-SPARSE (< 6 binaries/var), which is exactly the axis on
# which BOTH our elimination passes are gated off -- Factor (factor.cpp:206,
# threshold 6) and the during-search BVE dose (default_policy.cpp:940, same
# threshold 6, and BVE is also deferred out of preprocessing).  So on all four
# timeouts tactix runs with NO variable elimination and NO bounded variable
# addition at all, while kissat eliminates 40-86% of the variables.
# This wave FORCES those two passes on (the decisive test), prices the two
# unbounded rules (Strengthen / SubsumeForward have neither `dosed` nor
# `budget_aware`, so one round cannot be cut), and counts congruence merges.
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-final/tactix

line="$1"
IFS='|' read -r label stem cap arm <<<"$line"

f=$(ls "$R"/benchmarks/cache/tierB/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem"; exit 1; }
work=$TMPDIR/tbto2-${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }

echo "=== label=$label instance=$stem cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bytes=$(stat -c %s "$cnf")"
grep -m1 '^p cnf' "$cnf"

run() { /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$@" 2>&1 | grep -v '^v '; }

case "$arm" in
  shipped)     run "$BIN" "$cnf" ;;
  forcefactor) export TACTIX_FACTOR_DENSITY=0; run "$BIN" "$cnf" ;;
  forcebve)    export TACTIX_BVE_DOSE_DENSITY=0; run "$BIN" "$cnf" ;;
  forceboth)   export TACTIX_FACTOR_DENSITY=0 TACTIX_BVE_DOSE_DENSITY=0
               run "$BIN" "$cnf" ;;
  off-*)       export TACTIX_SLOT_DISABLE="${arm#off-}"; run "$BIN" "$cnf" ;;
  walk0)       export TACTIX_WALK_PERMILLE=0; run "$BIN" "$cnf" ;;
  alloff)      export TACTIX_NO_BVE=1 TACTIX_NO_FACTOR=1 \
                 TACTIX_SLOT_DISABLE=BCE,HyperTernaryRes,Instantiate,Vivify,Strengthen,SubsumeForward,Probe,Backbone,TransitiveReduce,Substitute,Congruence
               run "$BIN" "$cnf" ;;
  congtrace)   export TACTIX_CONG_TRACE=1; run "$BIN" "$cnf" ;;
  *)           echo "UNKNOWN ARM $arm"; exit 1 ;;
esac
echo "=== done $(date -Is)"
rm -rf "$work"
