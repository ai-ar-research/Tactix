#!/usr/bin/env bash
# Settle `mod4block_2vars_10gates_u2_autoenc-sc2009`: LOTTERY or CONSISTENTLY DEAD?
#
# The instance has timed out in three consecutive Tier B windows while kissat
# holds 18.7-20.9 s across 40+ recorded draws.  Every recorded tactix draw is a
# SINGLE draw of a DIFFERENT binary, so nothing on record separates "the
# distribution straddles the 300 s cap" from "the last day's work killed it".
#
# This probe measures a DISTRIBUTION per binary, three binaries in one window:
#   old  0418770  last recorded 13.98 s solve (2026-08-05 11:47)
#   mid  c7beaa0  first recorded TO (2026-08-06 01:00), pre ledger-wrap fix.
#                 Its binary sha 219b10d7... is byte-identical to the one that
#                 produced the recorded TO, so the draw set is comparable.
#   new  HEAD     post ledger-wrap fix + revisit-codebase pass
#
# Perturbations are SEARCH-side knobs only (chrono levels, reason-bump rate and
# limit, walk dose) -- mechanically independent of everything the last day
# changed (slot clocks/watermark/order, effort ledger, sweep governor, CCE,
# vivify reuse/tier3, eager subsume).  Same protocol as the Stage B gate's
# paired draws.  A draw counts only if the base trajectory actually moved, so
# every arm reports its own conflict count.
#
# Cap is 900 s, not the tier's 300 s, so a censored draw still reports WHERE in
# the tail it lands -- "straddles the cap" and "3x past the cap" are different
# verdicts and the tier cannot tell them apart.
#
# Manifest line: <label>|<bin>|<perturb>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188

IFS='|' read -r label bin perturb cap <<<"$1"

case "$bin" in
  old) BIN=$R/build/pin-mod4-old/tactix ;;
  mid) BIN=$R/build/pin-mod4-mid/tactix ;;
  new) BIN=$R/build/pin-mod4-diag/tactix ;;
  *)   echo "UNKNOWN BIN $bin"; exit 1 ;;
esac
[ -x "$BIN" ] || { echo "MISSING BINARY $BIN"; exit 1; }

f=/home/hwu/SAT-solving/benchmarks/cache/tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz
[ -r "$f" ] || { echo "MISSING INSTANCE $f"; exit 1; }

work=${TMPDIR:-/tmp}/m4lot-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/i.cnf
proof=$work/i.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

case "$perturb" in
  none)      ;;
  chrono85)  export TACTIX_CHRONO_LEVELS=85 ;;
  chrono90)  export TACTIX_CHRONO_LEVELS=90 ;;
  chrono95)  export TACTIX_CHRONO_LEVELS=95 ;;
  chrono105) export TACTIX_CHRONO_LEVELS=105 ;;
  chrono110) export TACTIX_CHRONO_LEVELS=110 ;;
  chrono115) export TACTIX_CHRONO_LEVELS=115 ;;
  br17)      export TACTIX_BUMP_REASONS_RATE=17 ;;
  br18)      export TACTIX_BUMP_REASONS_RATE=18 ;;
  br19)      export TACTIX_BUMP_REASONS_RATE=19 ;;
  br21)      export TACTIX_BUMP_REASONS_RATE=21 ;;
  br22)      export TACTIX_BUMP_REASONS_RATE=22 ;;
  br23)      export TACTIX_BUMP_REASONS_RATE=23 ;;
  bl8)       export TACTIX_BUMP_REASONS_LIMIT=8 ;;
  bl9)       export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bl11)      export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  bl12)      export TACTIX_BUMP_REASONS_LIMIT=12 ;;
  walk500)   export TACTIX_WALK_PERMILLE=500 ;;
  walk520)   export TACTIX_WALK_PERMILLE=520 ;;
  walk600)   export TACTIX_WALK_PERMILLE=600 ;;
  walk640)   export TACTIX_WALK_PERMILLE=640 ;;
  *)         echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac
export TACTIX_SLOT_STATS=1

echo "=== label=$label bin=$bin perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is)"
echo "=== bin-sha $(sha256sum "$BIN" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
grep -m1 '^p cnf' "$cnf"
echo "=== env: CHRONO=${TACTIX_CHRONO_LEVELS:-unset(100)}" \
     "BUMPRATE=${TACTIX_BUMP_REASONS_RATE:-unset(20)}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset(10)}" \
     "WALK=${TACTIX_WALK_PERMILLE:-unset(560)}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" "$cnf" "$proof" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c ticks|^c restarts|^c reductions|^c learned|^c slot|^c sweep|^c cce|^c vivify|^c eagersub|^c tiers|^c congy1|^c bve|^c factor|^c walk|^c chrono|^wall |maxrss'

echo "=== proof-bytes $(stat -c %s "$proof" 2>/dev/null || echo 0)"
echo "=== done $(date -Is)"
rm -rf "$work"
