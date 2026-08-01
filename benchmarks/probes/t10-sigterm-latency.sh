#!/usr/bin/env bash
# Task #10, §6 compliance: measure SIGTERM-to-exit LATENCY, the number the
# previous partial fix (`0a1e0aa`) did NOT move.
#
# The defect: `stop` was a Policy::solve parameter only, so no RULE could see
# it.  Polling at the phase round loop and slot boundaries bounds latency to ONE
# APPLICATION -- and one application is exactly the problem: `c timing` prices
# V6 Congruence at 131,327 ms over 3 applications on T105/T87, 89.5 % of a 146 s
# run that never reached search.  A competition harness escalates to SIGKILL and
# the PROOF IS LOST, so this is N2/§6, not a latency nuisance.
#
# The fix under test: Kernel::stop + kernel_terminating(), published by every
# policy through KernelStopPublisher, polled by Congruence at two SAFE points --
# after (read-only) extraction, and every 256 items of the closure worklist,
# where each completed merge is already fully applied and proof-logged.
#
# PRE-REGISTERED: the fix is GOOD only if latency drops to a small number of
# seconds AND the verdict/proof are unaffected when NOT interrupted (the
# identity task).  A latency that is merely somewhat lower is NOT a pass -- the
# whole point is bounding it below a harness's SIGKILL grace period.
#
# Manifest line: <label>|<binary>|<delay-before-SIGTERM>
set -u
R=/home/hwu/SAT-solving
line="$1"
IFS='|' read -r label bin delay <<<"$line"
BIN=$R/build/$bin/tactix

f=$(ls "$R"/benchmarks/cache/tierC/T105.2.0*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE"; exit 1; }
work=$TMPDIR/t10-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/T105.cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }

echo "=== label=$label bin=$bin delay=${delay}s host=$(hostname) date=$(date -Is)"

# BVE admission off is the configuration that produced the 131 s congruence
# applications (jobs 1575474/1575475).
export TACTIX_BVE_ADMIT=off

if [ "$delay" = "none" ]; then
  # Identity leg: no signal at all. Verdict + proof bytes must be unaffected.
  /usr/bin/time -f "wall %e s" timeout 1200 "$BIN" "$cnf" "$work/p.drat" 2>&1 | grep -v '^v '
  echo "proofbytes=$(stat -c %s "$work/p.drat" 2>/dev/null || echo NA)"
else
  "$BIN" "$cnf" "$work/p.drat" > "$work/out" 2>&1 &
  pid=$!
  sleep "$delay"
  if ! kill -0 $pid 2>/dev/null; then
    echo "RESULT: solver already exited before the signal — raise the delay"
    wait $pid; grep -E '^s |^c timing.*Congruence' "$work/out" | head -3
    rm -rf "$work"; exit 0
  fi
  t0=$(date +%s.%N)
  kill -TERM $pid
  wait $pid; rc=$?
  t1=$(date +%s.%N)
  echo "RESULT: sigterm_to_exit=$(echo "$t1 - $t0" | bc) s   exit_rc=$rc"
  echo "proofbytes=$(stat -c %s "$work/p.drat" 2>/dev/null || echo NA)"
  grep -E '^s ' "$work/out" | head -2
  grep -E '^c interrupt' "$work/out" | head -1
  grep -E '^c timing +(Congruence|solve)' "$work/out" | head -3
fi

echo "=== done $(date -Is)"
rm -rf "$work"
