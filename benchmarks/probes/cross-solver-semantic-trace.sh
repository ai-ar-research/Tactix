#!/usr/bin/env bash
# Short correctness-checked validation of the cross-solver trace mechanism.
set -u

ROOT=/home/hwu/SAT-solving
OUT=${CROSS_TRACE_OUT:?set CROSS_TRACE_OUT}
TACTIX=${CROSS_TRACE_TACTIX:-$ROOT/build/event-trace/tactix}
KISSAT=${CROSS_TRACE_KISSAT:-$ROOT/tools/solvers/variants/kissat-event-trace/worktree/build/kissat}
CROSS_TRACE_CASES=${CROSS_TRACE_CASES:-tr-n170-s053:20 tr-n220-s013:10}
CROSS_TRACE_TIMEOUT=${CROSS_TRACE_TIMEOUT:-20}
CROSS_TRACE_LIMIT=${CROSS_TRACE_LIMIT:-50000}
CROSS_TRACE_CONFLICT_FROM=${CROSS_TRACE_CONFLICT_FROM:-}
CROSS_TRACE_CONFLICT_TO=${CROSS_TRACE_CONFLICT_TO:-}
DRAT=$ROOT/tools/checkers/bin/drat-trim
CHECKMODEL=$ROOT/tools/fuzz/checkmodel
mkdir -p "$OUT"

run_one() {
  local name=$1 expected=$2 solver=$3 binary=$4 trace log proof rc
  local -a window
  window=()
  if [ -n "$CROSS_TRACE_CONFLICT_FROM" ]; then
    window+=(TACTIX_EVENT_TRACE_CONFLICT_FROM="$CROSS_TRACE_CONFLICT_FROM")
  fi
  if [ -n "$CROSS_TRACE_CONFLICT_TO" ]; then
    window+=(TACTIX_EVENT_TRACE_CONFLICT_TO="$CROSS_TRACE_CONFLICT_TO")
  fi
  trace=$OUT/$name-$solver.tsv
  log=$OUT/$name-$solver.log
  proof=$OUT/$name-$solver.drat
  if [ "$solver" = tactix ]; then
    env "${window[@]}" TACTIX_EVENT_TRACE="$trace" \
      TACTIX_EVENT_TRACE_LIMIT="$CROSS_TRACE_LIMIT" \
      timeout --preserve-status --signal=TERM --kill-after=5 "$CROSS_TRACE_TIMEOUT" \
      "$binary" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$log" 2>&1
    rc=$?
  else
    window=()
    if [ -n "$CROSS_TRACE_CONFLICT_FROM" ]; then
      window+=(KISSAT_EVENT_TRACE_CONFLICT_FROM="$CROSS_TRACE_CONFLICT_FROM")
    fi
    if [ -n "$CROSS_TRACE_CONFLICT_TO" ]; then
      window+=(KISSAT_EVENT_TRACE_CONFLICT_TO="$CROSS_TRACE_CONFLICT_TO")
    fi
    env "${window[@]}" KISSAT_EVENT_TRACE="$trace" \
      KISSAT_EVENT_TRACE_LIMIT="$CROSS_TRACE_LIMIT" \
      timeout --preserve-status --signal=TERM --kill-after=5 "$CROSS_TRACE_TIMEOUT" \
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
for spec in $CROSS_TRACE_CASES; do
  name=${spec%:*}
  expected=${spec##*:}
  case "$expected" in
    10|20) ;;
    *) echo "invalid expected exit code in CROSS_TRACE_CASES: $spec" >&2; exit 2 ;;
  esac
  run_one "$name" "$expected" tactix "$TACTIX"
  run_one "$name" "$expected" kissat "$KISSAT"
done

for spec in $CROSS_TRACE_CASES; do
  name=${spec%:*}
  python3 "$ROOT/tools/bench/compare_event_trace.py" \
    "$OUT/$name-tactix.tsv" "$OUT/$name-kissat.tsv" \
    --mode semantic --radius 2 --fixture "$name" \
    --tactix-commit WORKTREE --kissat-commit 8af8e56f174b \
    --json "$OUT/$name-first-divergence.json" \
    >"$OUT/$name-compare.log" 2>&1 || [ "$?" -eq 1 ]
done
