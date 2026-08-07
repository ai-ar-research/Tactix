#!/usr/bin/env bash
# One task of the block-2 ARM-SIDE reach readout.
#
# Why it exists beside block2-counters.sh: a mechanism that SHIPS OFF prints a
# zero in a shipped run for a structural reason, so the shipped-config probe
# cannot tell "this arm changes nothing" from "this arm was never switched on".
# For those rows the reach must be read from the ARM's own run — this one.
#
# The env fragment is part of the task line, so the knob is in the command
# exactly as it will be in the pricing window's `--checked-arm`.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-$R/benchmarks/cache}
IFS='|' read -r stem tier build cap label envs <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/block2arm-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
# shellcheck disable=SC2086 — $envs is a deliberate list of KEY=VALUE words.
env TACTIX_SLOT_STATS=1 $envs timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" \
    > "$w/out" 2>&1
rc=$?
echo "=== stem=$stem tier=$tier cap=${cap}s build=$build arm=$label env=$envs rc=$rc"
grep -E "^s |^c " "$w/out"
rm -rf "$w"
