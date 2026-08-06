#!/usr/bin/env bash
# SCHED-E1 behaviour-preservation gate: breaking `run_slot_phase` into
# per-entry helpers is pure restructuring, so DefaultPolicy must be
# BYTE-IDENTICAL — same trajectory (conflicts/decisions/propagations, the
# per-rule application counts) and the same proof, bit for bit.
#
# The instance set is chosen so every branch the split touches actually runs:
# sted5 sits BELOW Factor's density threshold (the declining path), 9vliw is
# BVE/Vivify-heavy, b22_1 and FmlaEquivChain reach many inprocessing phases.
# Manifest: <arm>|<stem>|<tier>|<build>|<cap>
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-/home/hwu/SAT-solving/benchmarks/cache}
IFS='|' read -r arm stem tier build cap <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/e1id-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
TACTIX_SLOT_STATS=1 timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem build=$build"
grep -E "^s |^c conflicts|^c decisions|^c propagations|^c inprocess|^c slot " "$w/out"
echo "proof-sha $(sha256sum "$w/i.drat" | cut -c1-32)  bytes $(stat -c %s "$w/i.drat")"
rm -rf "$w"
