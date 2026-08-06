#!/usr/bin/env bash
# The one actionable lead out of the mod4block ablation: with CCE disabled the
# HEAD binary solved in 8.34 s (195,449 conflicts) where base times out, and
# CCE's own measured yield on this instance is 3 clauses deleted for 186 M ticks
# across 82 rounds.  One draw on a may-not-gate instance is a rumour, so this
# puts {base, noCCE} to PAIRED draws under the same search-side perturbations,
# which are mechanically independent of CCE's guard and dosing.
#
# This can only answer "is the 8.34 s a fluke on this instance"; it CANNOT gate
# a change to CCE -- that needs a tier.
#
# Manifest line: <label>|<arm>|<perturb>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188
BIN=$R/build/pin-mod4-diag/tactix

IFS='|' read -r label arm perturb cap <<<"$1"

f=/home/hwu/SAT-solving/benchmarks/cache/tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz
work=${TMPDIR:-/tmp}/m4cce-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/i.cnf
proof=$work/i.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

case "$arm" in
  base)  ;;
  noCCE) export TACTIX_SLOT_DISABLE=CCE ;;
  *)     echo "UNKNOWN ARM $arm"; rm -rf "$work"; exit 1 ;;
esac
case "$perturb" in
  none)      ;;
  chrono85)  export TACTIX_CHRONO_LEVELS=85 ;;
  chrono90)  export TACTIX_CHRONO_LEVELS=90 ;;
  chrono115) export TACTIX_CHRONO_LEVELS=115 ;;
  br17)      export TACTIX_BUMP_REASONS_RATE=17 ;;
  br19)      export TACTIX_BUMP_REASONS_RATE=19 ;;
  br21)      export TACTIX_BUMP_REASONS_RATE=21 ;;
  br23)      export TACTIX_BUMP_REASONS_RATE=23 ;;
  bl8)       export TACTIX_BUMP_REASONS_LIMIT=8 ;;
  bl12)      export TACTIX_BUMP_REASONS_LIMIT=12 ;;
  *)         echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac
export TACTIX_SLOT_STATS=1

echo "=== label=$label arm=$arm perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is)"
echo "=== bin-sha $(sha256sum "$BIN" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
echo "=== env: DISABLE=${TACTIX_SLOT_DISABLE:-unset} CHRONO=${TACTIX_CHRONO_LEVELS:-unset(100)}" \
     "BUMPRATE=${TACTIX_BUMP_REASONS_RATE:-unset(20)}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset(10)}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" "$cnf" "$proof" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c restarts|^c slot CCE|^c cce|^c sweep |^wall '

echo "=== done $(date -Is)"
rm -rf "$work"
