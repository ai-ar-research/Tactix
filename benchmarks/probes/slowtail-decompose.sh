#!/usr/bin/env bash
# ONE instrument over the whole Tier B solved-but-slow tail: for each instance,
# tactix (with `c timing` per-rule ms + the slot table) beside kissat and
# cadical stats, all in ONE submission so contention cancels.
# Readouts wanted: conflicts, propagations/conflict, decisions/conflict,
# per-rule ms, slot vs search split.
# Manifest line: label|stem|cap|solver|arm     (arm: shipped | env:V=1[;V=2])
# The env list is ';'-separated, NOT ','-separated: TACTIX_SLOT_PERMILLE's own
# value uses commas, and splitting on those exports its tail as a bare word.
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-fix-b/tactix
KIS=$R/tools/solvers/bin/kissat
CAD=$R/tools/solvers/bin/cadical
line="$1"
IFS='|' read -r label stem cap solver arm <<<"$line"
f=$(ls "$R"/benchmarks/cache/tierB/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem"; exit 1; }
work=$TMPDIR/slowtail-${SLURM_JOB_ID:-$$}-${SLURM_ARRAY_TASK_ID:-0}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }
echo "=== label=$label instance=$stem solver=$solver cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bytes=$(stat -c %s "$cnf")"
grep -m1 '^p cnf' "$cnf"
if [ "$arm" != shipped ]; then
  IFS=';' read -ra kv <<<"${arm#env:}"
  for a in "${kv[@]}"; do export "$a"; done
  echo "=== env: ${arm#env:}"
fi
case "$solver" in
  ours) cmd="$BIN $cnf" ;;
  kis)  cmd="$KIS $cnf" ;;
  cad)  cmd="$CAD $cnf" ;;
  # kissat with every simplifier off -- the debug-performance step-3 ablation:
  # if its conflict count rises to ours, our SEARCH is competitive and the gap
  # is what simplification buys.
  kisoff) cmd="$KIS --simplify=false --preprocess=false --vivify=false --factor=false --congruence=false --sweep=false --substitute=false $cnf" ;;
  # ONE simplifier off at a time: the step-3 attribution.
  kisnoelim) cmd="$KIS --eliminate=false $cnf" ;;
  kisnofac)  cmd="$KIS --factor=false $cnf" ;;
  cadnoelim) cmd="$CAD --elim=false $cnf" ;;
  *) echo "BAD SOLVER $solver"; exit 1 ;;
esac
/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" $cmd 2>&1 | grep -v '^v '
echo "=== done $(date -Is)"
rm -rf "$work"
