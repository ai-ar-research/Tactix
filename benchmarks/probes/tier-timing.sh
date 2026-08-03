#!/usr/bin/env bash
# Per-instance RUNTIME BREAKDOWN across Tier B and C: where each solve's wall
# clock goes (search vs the inprocessing slot, and per rule). Diagnostic only —
# NO proof writing, so far cheaper than a scored run (Tier C's proof checking
# alone was 4,557 s). Timings are the solver's own `c timing` ledger.
# Manifest line: <tier>|<path>|<cap>
set -u
R=/home/hwu/SAT-solving
IFS='|' read -r tier rel cap <<<"$1"
f=$(ls "$R/benchmarks/cache/$tier/$(basename "$rel")" 2>/dev/null | head -1)
[ -f "$f" ] || { echo "MISSING $rel"; exit 0; }
w=$TMPDIR/tt-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" > "$w/i.cnf" 2>/dev/null || { echo "STAGE $rel"; exit 0; }
echo "=== $tier $(basename "$rel") cap=${cap}s"
timeout "$cap" "$R/build/pin-ship-rd/tactix" "$w/i.cnf" 2>&1 \
  | grep -E "^c timing|^c (conflicts|decisions)|^s "
echo "=== end"
rm -rf "$w"
