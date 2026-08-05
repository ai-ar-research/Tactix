#!/usr/bin/env bash
# SCHED-N1FIX gate: DefaultPolicy behaviour must be BYTE-IDENTICAL after moving
# Factor's density predicate out of the rule and into the policy. The advice
# field defaults to "no restriction", and the policy writes exactly the value the
# rule used to compute itself, so the same occasions must be declined at the same
# points and the proof must be identical bit for bit.
#
# sted5 is included deliberately: it sits BELOW the density threshold, so it is
# an instance where the gate actually fires. An identity check that only covers
# admitted instances would pass vacuously.
# Manifest: <arm>|<stem>|<tier>|<build>|<cap>
set -u
R=/home/hwu/SAT-solving
IFS='|' read -r arm stem tier build cap <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$R"/benchmarks/cache/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/n1id-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem build=$build"
grep -E "^s |^c conflicts|^c decisions|^c propagations|^c factor|^c extension" "$w/out"
echo "proof-sha $(sha256sum "$w/i.drat" | cut -c1-32)  bytes $(stat -c %s "$w/i.drat")"
rm -rf "$w"
