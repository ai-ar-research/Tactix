#!/usr/bin/env bash
# Two-fixture semantic trace for ORDER-DEFAULT-1.  Submit as an array 0-9;
# five arms per instance: Tactix reorder 0/1/2 and Kissat reorder 0/2.
set -u

ROOT=/home/hwu/SAT-solving
OUT=${REORDER_TRACE_OUT:?set REORDER_TRACE_OUT}
TACTIX=${REORDER_TRACE_TACTIX:-$ROOT/build/pin-reorder-trace/tactix}
KISSAT=${REORDER_TRACE_KISSAT:-$ROOT/kissat/build-log/kissat}
TASK=${SLURM_ARRAY_TASK_ID:?array task required}
CASE=$((TASK / 5))
ARM=$((TASK % 5))

if [ "$CASE" -eq 0 ]; then
  NAME=tr-n170-s053
else
  NAME=tr-n220-s013
fi
CNF=$ROOT/benchmarks/cache/tierA/$NAME.cnf
mkdir -p "$OUT"

if [ "$ARM" -le 2 ]; then
  LABEL=tactix-r$ARM
  TRACE=$OUT/$NAME-$LABEL.tsv
  LOG=$OUT/$NAME-$LABEL.log
  PROOF=$OUT/$NAME-$LABEL.drat
  env TACTIX_REORDER=$ARM \
      TACTIX_LUCKY_LATE=0 \
      TACTIX_PREPROCESS_FOLLOWUPS=0 \
      TACTIX_JUMP_REASONS=0 \
      TACTIX_FACTOR_CAND_ROUNDS=0 \
      TACTIX_TOUCH_REDUNDANT=0 \
      TACTIX_EVENT_TRACE="$TRACE" \
      TACTIX_EVENT_TRACE_LIMIT=50000 \
      timeout --preserve-status --signal=TERM --kill-after=5 10 \
      "$TACTIX" "$CNF" "$PROOF" >"$LOG" 2>&1
  STATUS=$?
else
  if [ "$ARM" -eq 3 ]; then MODE=0; else MODE=2; fi
  LABEL=kissat-r$MODE
  RAW=$OUT/$NAME-$LABEL.raw.log
  TRACE=$OUT/$NAME-$LABEL.tsv
  timeout --preserve-status --signal=TERM --kill-after=5 10 \
      "$KISSAT" --log=2 --reorder=$MODE "$CNF" >"$RAW" 2>&1
  STATUS=$?
  python3 "$ROOT/tools/bench/normalize_kissat_log.py" "$RAW" "$TRACE"
fi

printf '%s\t%s\t%s\n' "$NAME" "$LABEL" "$STATUS" >"$OUT/$NAME-$LABEL.status"
exit 0
