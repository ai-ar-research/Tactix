#!/usr/bin/env bash
# Short correctness-checked validation of the cross-solver trace mechanism.
set -u

ROOT=/home/hwu/SAT-solving
OUT=${CROSS_TRACE_OUT:?set CROSS_TRACE_OUT}
TACTIX=${CROSS_TRACE_TACTIX:-$ROOT/build/event-trace/tactix}
KISSAT=${CROSS_TRACE_KISSAT:-$ROOT/tools/solvers/variants/kissat-event-trace/worktree/build/kissat}
DRAT=$ROOT/tools/checkers/bin/drat-trim
CHECKMODEL=$ROOT/tools/fuzz/checkmodel
mkdir -p "$OUT"

run_one() {
  local name=$1 expected=$2 solver=$3 binary=$4 trace log proof rc
  trace=$OUT/$name-$solver.tsv
  log=$OUT/$name-$solver.log
  proof=$OUT/$name-$solver.drat
  if [ "$solver" = tactix ]; then
    env TACTIX_EVENT_TRACE="$trace" TACTIX_EVENT_TRACE_LIMIT=50000 \
      timeout --preserve-status --signal=TERM --kill-after=5 20 \
      "$binary" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$log" 2>&1
    rc=$?
  else
    env KISSAT_EVENT_TRACE="$trace" KISSAT_EVENT_TRACE_LIMIT=50000 \
      timeout --preserve-status --signal=TERM --kill-after=5 20 \
      "$binary" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$log" 2>&1
    rc=$?
  fi
  printf '%s\t%s\t%s\n' "$name" "$solver" "$rc" >>"$OUT/status.tsv"
  [ "$rc" -eq "$expected" ] || return 1
  if [ "$rc" -eq 10 ]; then
    "$CHECKMODEL" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$log" \
      >"$OUT/$name-$solver.model-check"
  else
    "$DRAT" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$OUT/$name-$solver.drat-check" 2>&1
    grep -q 's VERIFIED' "$OUT/$name-$solver.drat-check"
  fi
}

: >"$OUT/status.tsv"
run_one tr-n170-s053 20 tactix "$TACTIX"
run_one tr-n170-s053 20 kissat "$KISSAT"
run_one tr-n220-s013 10 tactix "$TACTIX"
run_one tr-n220-s013 10 kissat "$KISSAT"

for name in tr-n170-s053 tr-n220-s013; do
  python3 "$ROOT/tools/bench/compare_event_trace.py" \
    "$OUT/$name-tactix.tsv" "$OUT/$name-kissat.tsv" \
    --mode semantic --radius 2 --fixture "$name" \
    --tactix-commit WORKTREE --kissat-commit 8af8e56f174b \
    --json "$OUT/$name-first-divergence.json" \
    >"$OUT/$name-compare.log" 2>&1 || [ "$?" -eq 1 ]
done
