#!/usr/bin/env bash
# Diagnosis probe for the FOUR Tier B instances tactix cannot solve
# (window 20260729-220951: veer_axi 187x, or_randxor 48x, mp1-klieber2017s 48x,
# unsat-set-b-fclqcolor 36x) plus the two near-misses (DivS_862_11 7.0x,
# 2013113162201nw 12.1x).  debug-performance ladder, steps 0-3.
#
# Arms are driven by a manifest (one task per line, see tierB-timeouts.mf):
#     <label>|<instance-stem>|<cap-seconds>|<arm>
# The tactix binary is build/pin-final -- the EXACT copy that produced the
# 20260729-220951 window (git 77fc578, mtime 21:51 < run 22:09).  Arm
# `sanity` re-measures mod4block against its known baseline
# (12.79 s / 418,796 conflicts) before any other arm is trusted (the
# 2026-07-29 pinned-binary hazard).
#
# NOTE (hazard hit once, 2026-07-30): the staged CNF path MUST be unique per
# array task -- $TMPDIR is shared by every task on the node, so two arms on the
# same instance raced on one file and four tasks died with "no such file".
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-final/tactix
KISSAT=$R/tools/solvers/bin/kissat
CADICAL=$R/tools/solvers/bin/cadical

line="$1"
IFS='|' read -r label stem cap arm <<<"$line"

f=$(ls "$R"/benchmarks/cache/tierB/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem"; exit 1; }
work=$TMPDIR/tbto-${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }

echo "=== label=$label instance=$stem cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bytes=$(stat -c %s "$cnf")"
grep -m1 '^p cnf' "$cnf"

run() { /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$@" 2>&1 | grep -v '^v '; }

case "$arm" in
  # ---- tactix, shipped default ------------------------------------------
  shipped|sanity)   run "$BIN" "$cnf" ;;
  shipped-proof)    run "$BIN" "$cnf" "$work/proof.drat"; ls -l "$work/proof.drat" ;;
  # ---- tactix ablations (DIAGNOSTIC only; a rule-off arm is never a default)
  off-*)     export TACTIX_SLOT_DISABLE="${arm#off-}"; run "$BIN" "$cnf" ;;
  nobve)     export TACTIX_NO_BVE=1; run "$BIN" "$cnf" ;;
  nofactor)  export TACTIX_NO_FACTOR=1; run "$BIN" "$cnf" ;;
  walk0)     export TACTIX_WALK_PERMILLE=0; run "$BIN" "$cnf" ;;
  alloff)    export TACTIX_NO_BVE=1 TACTIX_NO_FACTOR=1 \
               TACTIX_SLOT_DISABLE=BCE,HyperTernaryRes,Instantiate,Vivify,Strengthen,SubsumeForward,Probe,Backbone,TransitiveReduce,Substitute,Congruence
             run "$BIN" "$cnf" ;;
  trio-off)  export TACTIX_SLOT_DISABLE=BCE,HyperTernaryRes,Instantiate
             run "$BIN" "$cnf" ;;
  # ---- baselines ---------------------------------------------------------
  kissat)    run "$KISSAT" "$cnf" ;;
  kissat-no-*) run "$KISSAT" "--no-${arm#kissat-no-}" "$cnf" ;;
  kissat-alloff)
             run "$KISSAT" --no-sweep --no-congruence --no-factor --no-eliminate \
                 --no-vivify --no-probe --no-substitute --no-definitions "$cnf" ;;
  cadical)   run "$CADICAL" "$cnf" ;;
  cadical-no-*) run "$CADICAL" "--no-${arm#cadical-no-}" "$cnf" ;;
  *)         echo "UNKNOWN ARM $arm"; exit 1 ;;
esac
echo "=== done $(date -Is)"
rm -rf "$work"
