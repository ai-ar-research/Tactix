#!/usr/bin/env bash
# §6: measure SIGTERM-to-exit latency. `timeout` sends TERM at the cap; we record
# how long the process actually takes to die. Before the 2026-07-30 fix the
# inprocessing slot never polled `stop`, so T105/T87 under BVE admission=none
# ignored TERM for ~1,100 s and were killed by the SLURM wall having printed
# nothing. A competition harness escalates to SIGKILL and the proof is lost.
# PASS = exits within a few seconds of the cap AND still prints `s UNKNOWN`.
set -u
R=/home/hwu/SAT-solving
bin="$1"; inst="$2"; tier="$3"; cap="$4"
f=$(ls $R/benchmarks/cache/$tier/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/sig.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
echo "=== $bin $inst cap=${cap}s (admission=none, the config that exposed it) ==="
t0=$(date +%s)
TACTIX_BVE_ADMIT=none timeout "$cap" $R/build/$bin/tactix "$cnf" 2>&1 | grep -E "^s |^c conflicts" | head -3
t1=$(date +%s)
echo "elapsed=$((t1-t0))s  cap=${cap}s  SIGTERM-to-exit lag=$((t1-t0-cap))s"
rm -f "$cnf"
