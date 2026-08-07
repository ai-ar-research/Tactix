#!/usr/bin/env bash
# WHERE does the solver stop polling? Locate it instead of guessing: start the
# solve, let it run, send SIGTERM, and if it is still alive N seconds later take
# stack samples. The TransitiveReduce per-dst poll is live (polls == checked on
# three other instances) yet `veer_axi` still ignored a 300 s SIGTERM, so the
# unpolled section is elsewhere and the stack says where.
#
# Manifest: <stem>|<tier>|<build>|<run_s>|<grace_s>|<samples>
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-$R/benchmarks/cache}
IFS='|' read -r stem tier build run grace samples <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/v4hang-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"

env TACTIX_SLOT_STATS=1 "$BIN" "$w/i.cnf" "$w/i.drat" > "$w/out" 2>&1 &
pid=$!
echo "=== stem=$stem pid=$pid run=${run}s grace=${grace}s"

sleep "$run"
if ! kill -0 "$pid" 2>/dev/null; then
  echo "SOLVED BEFORE SIGTERM (wall < ${run}s)"; wait "$pid"; grep -E "^s |^c transitive" "$w/out"; rm -rf "$w"; exit 0
fi

# Sample BEFORE the signal: where does it spend its time at all?
for i in $(seq 1 "$samples"); do
  echo "--- pre-TERM sample $i"
  eu-stack -p "$pid" 2>/dev/null | head -25 || echo "(eu-stack failed)"
  sleep 3
done

echo "=== sending SIGTERM"
kill -TERM "$pid" 2>/dev/null
sleep "$grace"
if ! kill -0 "$pid" 2>/dev/null; then
  echo "EXITED WITHIN ${grace}s OF SIGTERM — poll is reached"
else
  echo "STILL ALIVE ${grace}s AFTER SIGTERM — the unpolled section is below"
  for i in $(seq 1 "$samples"); do
    echo "--- post-TERM sample $i"
    eu-stack -p "$pid" 2>/dev/null | head -25 || echo "(eu-stack failed)"
    sleep 3
  done
  kill -9 "$pid" 2>/dev/null
fi
wait "$pid" 2>/dev/null
grep -E "^s |^c conflicts|^c transitive|^c bve |^c sweep |^c factor" "$w/out" || true
rm -rf "$w"
