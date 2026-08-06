#!/usr/bin/env bash
# SCHED-G2 + SCHED-H1 (C1/C2 half) — counters only, on real instances.
# Manifest line: <arm>|<stem>|<tier>|<build>|<cap>
#   on        shipped. C1 deletes; `forward_subsumed` is its yield.
#   shadow    TACTIX_SUBSUME_SHADOW=1. C1 records instead of deleting, so the
#             trajectory is the C1-LESS one and `c c1shadow` reports how much of
#             C1's yield Strengthen picks up by itself (SCHED-G2's whole answer).
#   sigterm   short cap, so `timeout` really does SIGTERM a running solve —
#             the REAL §6 path. `c c1c2stop` must be non-zero for some instance
#             or the H1 leg is vacuous.
#   stopfuzz  TACTIX_STOP_FUZZ, the deterministic stress cadence: it bypasses
#             the 256-poll window, so every site is reachable on a short round.
# The wall clock here is not a measurement; this session's allocation is
# oversubscribed and only counters are contention-invariant.
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
IFS='|' read -r arm stem tier build cap <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls /home/hwu/SAT-solving/benchmarks/cache/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
w=${TMPDIR:-/tmp}/g2-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
[ "$arm" = "shadow" ] && export TACTIX_SUBSUME_SHADOW=1
[ "$arm" = "stopfuzz" ] && export TACTIX_STOP_FUZZ=200000
timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem cap=$cap rc=$?"
grep -E "^s |^c conflicts|^c subsume |^c strengthen |^c c1shadow|^c c1c2stop|^c interrupt|^c budgetstop" "$w/out"
rm -rf "$w"
