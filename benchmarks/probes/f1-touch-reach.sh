#!/usr/bin/env bash
# SCHED-F1 reachability + NON-WEDGE witness for the touched-epoch filters.
# Manifest line: <arm>|<stem>|<tier>|<build>|<cap>
# Reads counters only (contention-invariant); the wall clock here is not a
# measurement.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
IFS='|' read -r arm stem tier build cap <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls /home/hwu/SAT-solving/benchmarks/cache/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
w=${TMPDIR:-/tmp}/f1-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
[ "$arm" = "off" ] && export TACTIX_TOUCH_MARKS=0
[ "$arm" = "redon" ] && export TACTIX_TOUCH_REDUNDANT=1
export TACTIX_SLOT_STATS=1
timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem"
grep -E "^s |^c conflicts|^c touch|^c touch3|^c touchred|^c tiers|^c subhold|^c subsume|^c strengthen|^c bce|^c ternary|^c instantiate|^c budgetstop" "$w/out"
grep -E "^c slot " "$w/out" | grep -E "SubsumeForward|Strengthen|BCE|HyperTernaryRes|Instantiate"
grep -c "UNACCOUNTED" "$w/out" | sed 's/^/unaccounted-lines /'
rm -rf "$w"
