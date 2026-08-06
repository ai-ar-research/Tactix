#!/usr/bin/env bash
# The fairness half of the mod4block verdict.
#
# The knob-perturbation array (mod4block-lottery.sh) shows tactix straddling the
# cap under search-side perturbations, but that alone cannot say whether the
# INSTANCE is a lottery or only OUR configuration is: kissat's stable 18.7-20.9 s
# across 40 recorded windows is 40 replays of ONE deterministic trajectory on ONE
# byte-identical CNF, not 40 draws.
#
# A satisfiability-preserving CNF permutation (variable renaming + polarity flip
# + clause/literal shuffle) is inert for EVERY solver and independent of all
# solver code, so it measures the instance's intrinsic hardness distribution.
# Seed 0 is the identity, i.e. the control.
#
# Manifest line: <label>|<solver>|<seed>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188

IFS='|' read -r label solver seed cap <<<"$1"

f=/home/hwu/SAT-solving/benchmarks/cache/tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz
work=${TMPDIR:-/tmp}/m4perm-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
raw=$work/raw.cnf
cnf=$work/i.cnf
proof=$work/i.drat
xz -dc "$f" >"$raw" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }
# The run of 2026-08-06 used the byte-identical copy under scratch/ (sha256
# 9ed9b45f...); the permuter is committed next to this script because it is
# reusable for every may-not-gate instance, not just this one.
python3 "$R/benchmarks/probes/permute_cnf.py" "$raw" "$cnf" "$seed" \
  || { echo "PERMUTE FAILED"; rm -rf "$work"; exit 1; }

case "$solver" in
  tactix)  CMD=("$R/build/pin-mod4-diag/tactix" "$cnf" "$proof") ;;
  kissat)  CMD=(/home/hwu/SAT-solving/tools/solvers/bin/kissat "$cnf") ;;
  cadical) CMD=(/home/hwu/SAT-solving/tools/solvers/bin/cadical "$cnf") ;;
  *)       echo "UNKNOWN SOLVER $solver"; rm -rf "$work"; exit 1 ;;
esac

echo "=== label=$label solver=$solver seed=$seed cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is)"
echo "=== bin-sha $(sha256sum "${CMD[0]}" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
grep -m1 '^p cnf' "$cnf"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "${CMD[@]}" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c restarts|^c sweep|^c factor|^c bve|^c walk|^c conflicts:|^c decisions:|^c propagations:|^c process-time|^wall |maxrss'

echo "=== proof-bytes $(stat -c %s "$proof" 2>/dev/null || echo 0)"
echo "=== done $(date -Is)"
rm -rf "$work"
