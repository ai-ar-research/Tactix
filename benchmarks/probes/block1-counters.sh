#!/usr/bin/env bash
# Phase-4 block-1 desk triage: decide whether the D3 and E3 pricing arms deserve
# a Tier B+C window, on counters only. No timing claim is made or readable here.
#
# NEITHER ARM IS SET. Both are read off the shipped binary's own counters; the
# only env var is TACTIX_SLOT_STATS, a print switch (default_policy.cpp), not an
# arm — and it goes in the command line, so the knob-in-the-command rule holds.
#
# PRE-REGISTERED CRITERIA, both decided before the run:
#  D3 (TACTIX_SLOT_WATERMARK) retires iff `blocked` is 0 on BOTH clocks on every
#     instance. slot_watermark_'s only consumer is the `gated` term in
#     want_slot_phase and blocked_episodes increments on exactly the branch that
#     suppresses a phase, so blocked==0 proves the WATERMARK=0 arm is
#     bit-identical. A non-zero anywhere and the arm earns its window.
#  E3 (TACTIX_EFFORT_CEIL) CANNOT be decided by `clamped` alone — a zero there is
#     arithmetically forced at this scale, which is why the denominator is
#     printed beside it. Only BVE (2e9), Strengthen and SubsumeForward (1e8)
#     carry a ceiling, all at 100 permille, and delta = reference/1000*permille
#     passes them only above reference 2e10 / 1e9. So E3 retires FROM THE TIER
#     B/C TABLE iff clamped==0 AND max(global ticks) stays well under 1e9.
#     That is a scale statement, not "inert": at Tier D's 5000 s cap the same
#     arithmetic puts reference near 1.4e9, over the subsume ceiling, so E3 is
#     re-examined before the Tier D campaign whatever this probe says.
#
# The predecessor probe (e2e3-arms.sh) greps '^c slot ' with a trailing space,
# which never matches 'c slot-clock' — so D3's counters have never been printed
# by any probe. That is the reason this exists rather than a re-read.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-$R/benchmarks/cache}
IFS='|' read -r stem tier build cap <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/block1-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
env TACTIX_SLOT_STATS=1 timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" \
    > "$w/out" 2>&1
echo "=== stem=$stem tier=$tier cap=${cap}s build=$build"
grep -E "^s |^c conflicts" "$w/out"
# D3: both clocks' blocked counters. Note NO trailing space in the pattern.
grep -E "^c slot-clock" "$w/out"
# E3: the clamp columns for every metered entry, plus the denominator that
# decides whether a zero means anything.
grep -E "^c slot " "$w/out" | awk '{printf "%s %s\n", $3, substr($0, index($0,"maxeff"))}'
grep -E "^c meters" "$w/out"
rm -rf "$w"
