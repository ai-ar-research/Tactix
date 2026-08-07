#!/usr/bin/env bash
# TransitiveReduce interior poll (kissat transitive.c:181-183), three questions
# in one array. Manifest: <mode>|<stem>|<tier>|<build>|<cap>|<env...>
#
#  term  — does a SIGTERM now stop the solver PROMPTLY? `timeout -s TERM <cap>`
#          then report the wall actually taken. Before the poll landed, a
#          `veer_axi` source ran ~10 min past a 300 s cap and only SLURM's wall
#          ended it. PASS = wall is within a few seconds of the cap.
#  ab    — the dose A/B: shipped (live meter) vs TACTIX_TRANSITIVE_LIVE_METER=0,
#          the permanent baseline arm. Read `c transitive` (ticks, polls,
#          terminated) and `c slot TransitiveReduce` (handed vs spent) FIRST:
#          an arm whose counters did not move was not tested.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-$R/benchmarks/cache}
IFS='|' read -r mode stem tier build cap envs <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/v4poll-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
start=$(date +%s)
env TACTIX_SLOT_STATS=1 $envs timeout -s TERM "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" \
    > "$w/out" 2>&1
rc=$?
wall=$(( $(date +%s) - start ))
echo "=== mode=$mode stem=$stem env='$envs' cap=${cap}s"
echo "wall ${wall}s rc=$rc  overshoot=$(( wall - cap ))s"
grep -E "^s |^c conflicts" "$w/out"
grep -E "^c transitive" "$w/out"
grep -E "^c slot TransitiveReduce" "$w/out" || true
grep -E "^c meters" "$w/out"
echo "proof-bytes $(stat -c %s "$w/i.drat" 2>/dev/null || echo 0)"
rm -rf "$w"
