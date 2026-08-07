#!/usr/bin/env bash
# One task of the block-2 anti-vacuity readout: solve one instance with the
# SHIPPED configuration and keep its entire `c ` block.
#
# The proof is written and thrown away: a run that does not write one is a
# different workload (plan §8.1), and several of the counters read here move
# only on the proof-emitting path.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-$R/benchmarks/cache}
IFS='|' read -r stem tier build cap <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/block2-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
env TACTIX_SLOT_STATS=1 timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" \
    > "$w/out" 2>&1
rc=$?
echo "=== stem=$stem tier=$tier cap=${cap}s build=$build rc=$rc"
grep -E "^s |^c " "$w/out"
rm -rf "$w"
