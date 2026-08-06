#!/usr/bin/env bash
# Companion to mod4block-lottery.sh: if HEAD is CONSISTENTLY dead on
# `mod4block_2vars_10gates_u2_autoenc-sc2009`, which of the last day's features
# owns it?  One binary (HEAD), one feature backed out per arm, all arms in ONE
# submission so contention cancels.  Counters are the primary read; the wall
# time is secondary.
#
# Also carries the two references at the same cap, so the tail is placed against
# kissat's 18.7-20.9 s and cadical's 69-78 s rather than against nothing.
#
# Manifest line: <label>|<arm>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188
BIN=$R/build/pin-mod4-diag/tactix

IFS='|' read -r label arm cap <<<"$1"

f=/home/hwu/SAT-solving/benchmarks/cache/tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz
work=${TMPDIR:-/tmp}/m4abl-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/i.cnf
proof=$work/i.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

CMD=("$BIN" "$cnf" "$proof")
case "$arm" in
  base)        ;;
  clocksoff)   export TACTIX_SLOT_CLOCKS=0 ;;
  clockson)    export TACTIX_SLOT_CLOCKS=1 ;;
  watermarkoff) export TACTIX_SLOT_WATERMARK=0 ;;
  orderlegacy) export TACTIX_SLOT_ORDER=legacy ;;
  noeagersub)  export TACTIX_EAGER_SUBSUME=0 ;;
  notouchmark) export TACTIX_TOUCH_MARKS=0 ;;
  novivreuse)  export TACTIX_VIVIFY_REUSE=0 ;;
  novivtier3)  export TACTIX_VIVIFY_TIER3=0 ;;
  nosweepbud)  export TACTIX_SWEEP_ENV_BUDGET=0 ;;
  nofacdemote) export TACTIX_FACTOR_DEMOTE=0 ;;
  noCCE)       export TACTIX_SLOT_DISABLE=CCE ;;
  noSweep)     export TACTIX_SLOT_DISABLE=Sweep ;;
  noCong)      export TACTIX_SLOT_DISABLE=Congruence ;;
  noBCE)       export TACTIX_SLOT_DISABLE=BCE ;;
  noStr)       export TACTIX_SLOT_DISABLE=Strengthen ;;
  noSubF)      export TACTIX_SLOT_DISABLE=SubsumeForward ;;
  noInst)      export TACTIX_SLOT_DISABLE=Instantiate ;;
  noVivify)    export TACTIX_SLOT_DISABLE=Vivify ;;
  kissat)      CMD=(/home/hwu/SAT-solving/tools/solvers/bin/kissat "$cnf") ;;
  cadical)     CMD=(/home/hwu/SAT-solving/tools/solvers/bin/cadical "$cnf") ;;
  *)           echo "UNKNOWN ARM $arm"; rm -rf "$work"; exit 1 ;;
esac
export TACTIX_SLOT_STATS=1

echo "=== label=$label arm=$arm cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is)"
echo "=== bin-sha $(sha256sum "${CMD[0]}" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
echo "=== env: CLOCKS=${TACTIX_SLOT_CLOCKS:-unset} WATERMARK=${TACTIX_SLOT_WATERMARK:-unset}" \
     "ORDER=${TACTIX_SLOT_ORDER:-unset} EAGERSUB=${TACTIX_EAGER_SUBSUME:-unset}" \
     "TOUCH=${TACTIX_TOUCH_MARKS:-unset} VIVREUSE=${TACTIX_VIVIFY_REUSE:-unset}" \
     "VIVT3=${TACTIX_VIVIFY_TIER3:-unset} SWEEPBUD=${TACTIX_SWEEP_ENV_BUDGET:-unset}" \
     "FACDEM=${TACTIX_FACTOR_DEMOTE:-unset} DISABLE=${TACTIX_SLOT_DISABLE:-unset}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "${CMD[@]}" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c ticks|^c restarts|^c reductions|^c learned|^c slot|^c sweep|^c cce|^c vivify|^c eagersub|^c tiers|^c congy1|^c bve|^c factor|^c walk|^c chrono|^c conflicts:|^c decisions:|^c propagations:|^c process-time|^wall |maxrss'

echo "=== proof-bytes $(stat -c %s "$proof" 2>/dev/null || echo 0)"
echo "=== done $(date -Is)"
rm -rf "$work"
