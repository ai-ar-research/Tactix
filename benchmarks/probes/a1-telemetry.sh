#!/usr/bin/env bash
# SCHED-A1 gate: the telemetry must be behaviour-preserving (byte-identical
# proof + identical trajectory vs the pre-A1 build), AND non-vacuous — the
# `c slot` lines must actually appear and account for every occasion.
# Manifest: <arm>|<stem>|<tier>|<build>|<cap>|<slotstats>
set -u
R=/home/hwu/SAT-solving
IFS='|' read -r arm stem tier build cap slot <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$R"/benchmarks/cache/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
w=${TMPDIR:-/tmp}/a1-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
[ "$slot" = "1" ] && export TACTIX_SLOT_STATS=1
timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem build=$build slotstats=$slot"
grep -E "^s |^c conflicts|^c decisions|^c propagations" "$w/out"
echo "proof-sha $(sha256sum "$w/i.drat" | cut -c1-32)"
grep -c "^c slot " "$w/out" | sed 's/^/slot-lines /'
grep -E "^c slot " "$w/out" | head -20
rm -rf "$w"
