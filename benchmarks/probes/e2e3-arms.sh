#!/usr/bin/env bash
# SCHED-E2 / SCHED-E3 reachability. Neither item may be banked on a green run:
#  E2 — each per-rule ordering arm must show `moved > 0` on the `c cand-order`
#       line, or the arm did not reorder anything and was not tested.
#  E3 — the effort CEILING must show `clamped > 0` when it is set small enough
#       to bind, or the clamp is dead code; and the SHIPPED constants' clamp
#       count is the measurement that decides whether the ledger row closes as
#       a port or as an accepted deviation.
# Manifest: <arm>|<stem>|<tier>|<build>|<cap>|<env...>
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-/home/hwu/SAT-solving/benchmarks/cache}
IFS='|' read -r arm stem tier build cap envs <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/e2e3-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
env TACTIX_SLOT_STATS=1 $envs timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" \
    > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem env='$envs'"
grep -E "^s |^c conflicts|^c cand-order" "$w/out"
# only the metered entries carry a ceiling; print the clamp columns for all
grep -E "^c slot " "$w/out" | sed 's/.*| handed/handed/'  | head -0
grep -E "^c slot " "$w/out" | awk '{printf "%s %s\n", $3, substr($0, index($0,"maxeff"))}'
echo "proof-sha $(sha256sum "$w/i.drat" | cut -c1-32)  bytes $(stat -c %s "$w/i.drat")"
rm -rf "$w"
